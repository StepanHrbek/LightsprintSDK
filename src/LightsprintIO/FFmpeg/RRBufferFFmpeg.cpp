// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Video load & play using FFmpeg/libav and portaudio.
// When you open video or audio, it is stopped.
// Resolution and duration (as provided by ffmpeg) are known immediately,
// but contents of first frame is not avaiable until background thread
// loads it (it can take fraction of second).
//
// Why ffmpeg/libav?
// - multiplatform
// - at least potential for GPU accel later (proprietary APIs have better GPU accel, but they are not multiplatform)
//
// Why portaudio?
// a) fmod... no, i had it working in lightsmark, but binary blob is ugly
// b) generic audio callback, so that user can plug any backend... no, nice but too complicated
// c) sdl... no, it has the best samples, but it's not just audio, has many dependencies
// d) sfml... no, it can replace glut in my samples, but it's not just audio, has many dependencies. also missing in macports
// e) libao... good, gpl
// f) openal... good, lgpl
// g) portaudio... good, mit
//
// Note that Lightsprint SDK contains also RRBufferDirectShow, video playback using DirectShow.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#if defined(SUPPORT_FFMPEG) || defined(SUPPORT_LIBAV)

#include "RRBufferFFmpeg.h"
#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRDebug.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <queue>

// audio (by portaudio)
#include "portaudio.h"
#if defined(_M_X64) || defined(_LP64)
	#pragma comment(lib,"portaudio_x64.lib")
#else
	#pragma comment(lib,"portaudio_x86.lib")
#endif

// video (by ffmpeg or libav)
extern "C"
{
	// missing header? install FFmpeg/libav or disable SUPPORT_FFMPEG in supported_formats.h
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/avutil.h>
	#include <libavutil/pixdesc.h>
	#include <libavutil/time.h>
	#include <libswscale/swscale.h>
}
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avdevice.lib")
#pragma comment(lib,"avfilter.lib")
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"swscale.lib")

using namespace rr;

#define MAX_IMAGE_QUEUE_LENGTH 30
#define MAX_PACKET_QUEUE_LENGTH 30
#define SEEK_SECONDS_BACK 0 // it seems that seek(x) goes to first keyframe after x, so after seek(x) we might be at x+2. if we want x exactly, we can try seek(x-few seconds), then video_proc automatically decodes until reaching x
#define SKIP_SEEK_SECONDS_FORWARD 2 // when stopped and seek(x) means going little bit forward, do nothing, video_proc automatically decodes until reaching x

#ifdef SUPPORT_LIBAV
	#define LIB_NAME "libav"
#else
	#define LIB_NAME "ffmpeg"
#endif


//////////////////////////////////////////////////////////////////////////////
//
// AVPacketWrapper

//! C++ wrapper for AVPacket
struct AVPacketWrapper : public AVPacket
{
	AVPacketWrapper()
	{
		av_new_packet(this,0);
	}
	virtual ~AVPacketWrapper()
	{
		av_free_packet(this);
	}
};

//! Optional extension, for debugging.

//! demux_proc can send nullptr or SeekPacket to tell audio/video_proc to seek, both work.
//! SeekPacket holds extra data for debugging.
struct SeekPacket : public AVPacketWrapper
{
	SeekPacket(double _seekSeconds)
	{
		seekSeconds = _seekSeconds;
	}
	double seekSeconds;
};


//////////////////////////////////////////////////////////////////////////////
//
// Queue

//! Generic thread safe queue.
template<class C>
class Queue
{
public:
	// for packet queues, pop is blocking
	void push(C c)
	{
		std::lock_guard<std::mutex> lock(mutex);
		queue.push(c);
		cond.notify_one();
	}
	C blocking_pop(bool& aborting)
	{
		std::unique_lock<std::mutex> lock(mutex);
		while (!aborting)
		{
			if (!queue.empty())
			{
				C result = queue.front();
				queue.pop();
				return result;
			}
			cond.wait(lock); // blocking [#61], [#62]
		}
		return nullptr;
	}

	// for image queue, push is blocking
	void blocking_push(C c, bool& aborting)
	{
		std::unique_lock<std::mutex> lock(mutex);
		while (!aborting && queue.size()>=MAX_IMAGE_QUEUE_LENGTH)
			cond.wait(lock); // blocking [#50]
		queue.push(c);
	}
	// pop all older elements, return the newest one of them (delete others)
	C pop_older(double seconds)
	{
		C result = nullptr;
		std::lock_guard<std::mutex> lock(mutex);
		while (!queue.empty() && (queue.front()->pts<seconds || !queue.front()->dbg_numImagesSinceSeek)) // always pop if it is first frame after seek
		{
			delete result;
			result = queue.front();
			queue.pop();
		}
		if (result)
			cond.notify_one();
		return result;
	}

	void clear()
	{
		std::lock_guard<std::mutex> lock(mutex);
		while (!queue.empty())
		{
			delete queue.front();
			queue.pop();
		}
		cond.notify_one(); // wake up [#50], [#61], [#62]
	}
	size_t size()
	{
		return queue.size();
	}
	~Queue()
	{
		clear();
	}

private:
	std::queue<C> queue;
	std::mutex mutex;
	std::condition_variable cond;
};


//////////////////////////////////////////////////////////////////////////////
//
// VideoPicture

struct VideoPicture
{
	//AVFrame* avFrame;
	RRBuffer* buffer;
	double pts;
	// debugging info
	double dbg_seekSeconds;
	unsigned dbg_numImagesSinceStart;
	unsigned dbg_numImagesSinceSeek;

	// _format can be calculated from _avFrame, but caller already knows it
	VideoPicture(AVFrame*& _avFrame, double _pts, rr::RRBufferFormat _format)
	{
		//avFrame = _avFrame;
		//_avFrame = NULL;
		buffer = RRBuffer::create(BT_2D_TEXTURE, _avFrame->width, _avFrame->height, 1, _format, true, nullptr);
//		buffer = RRBuffer::create(BT_2D_TEXTURE, _avFrame->width, _avFrame->height, 1, BF_LUMINANCE, true, _avFrame->data[0]); // SDL_YV12_OVERLAY, screen
//		sws_scale(video_swsContext, (uint8_t const * const *)avFrame->data, avFrame->linesize, 0, video_avCodecContext->height, avFrameRGB->data, avFrameRGB->linesize);
		pts = _pts;
	}
	~VideoPicture()
	{
		//av_frame_free(&avFrame);
		delete buffer;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// Player

//! FFmpeg player that sends sound to portaudio, image to RRBuffer.
class FFmpegPlayer
{
public:

	FFmpegPlayer(const RRString& _filename)
	{
		looping = true;
		playing = false;
		width = 0;
		height = 0;
		format = rr::BF_RGB;
		duration = -1; // unknown
		stoppedSecondsFromStart = 0;
		seekSecondsFromStart = -1; // no seek
		aborting = false;

		demux_working = false;
		avFormatContext = nullptr;

		audio_streamIndex = -1;
		audio_avStream = nullptr;
		audio_avCodecContext = nullptr;

		video_streamIndex = -1;
		video_avStream = nullptr;
		video_avCodecContext = nullptr;
		video_swsContext = nullptr;

		image_visible = nullptr;

		// open file and start decoding first frame on background
		av_register_all();		
		open_file(_filename); // we can call this from demux_thread, but we prefer blocking until width/height/duration are known
		if (hasAudio() || hasVideo()) // hasXxx works only after open_file()
			demux_thread = std::thread(&FFmpegPlayer::demux_proc, this);
	}

	virtual ~FFmpegPlayer()
	{
		// stop threads
		aborting = true; // starts closing down all threads
		if (demux_thread.joinable())
			demux_thread.join();
		if (audio_thread.joinable())
		{
			audio_packetQueue.clear(); // wake up audio_proc if blocked in [#62]
			audio_thread.join();
		}
		if (video_thread.joinable())
		{
			image_queue.clear(); // wake up video_proc if blocked in [#50]
			video_packetQueue.clear(); // wake up video_proc if blocked in [#61]
			video_thread.join();
		}

		// ffmpeg audio (producer)
		audio_avStream = nullptr;
		audio_avCodecContext = nullptr;

		// ffmpeg video (producer)
		video_avStream = nullptr;
		video_avCodecContext = nullptr;

		// yuv images (consumer)
		RR_SAFE_DELETE(image_visible);
	}

	bool hasAudio()
	{
		return audio_avCodecContext;
	}

	bool hasVideo()
	{
		return video_avCodecContext;
	}

	/////////////////////////////////////////////////////////////////////////
	// audio_proc
	/////////////////////////////////////////////////////////////////////////

	int audio_proc()
	{
		bool pa_started = false;
		PaStream* pa_stream = nullptr;

		PaError err = Pa_Initialize();
		if (err!=paNoError)
		{
			rr::RRReporter::report(rr::WARN,"Pa_Initialize()=%d\n",(int)err);
			return -1;
		}

		/* look for the best device for our data
		int numDevices = Pa_GetDeviceCount();
		if (numDevices<0)
		{
			rr::RRReporter::report(rr::WARN,"Pa_GetDeviceCount()=%d\n",(int)numDevices);
			return -1;
		}
		for (i=0;i<numDevices;i++)
		{
			const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
			...
		}*/

		bool convert_to_planar = false;
		bool convert_to_interleaved = false;
		char* buf = nullptr;
		unsigned bufBytes = 0;

		PaSampleFormat pa_sampleFormat = convert(audio_avCodecContext->sample_fmt);
		err = Pa_OpenDefaultStream(&pa_stream, 0, audio_avCodecContext->channels, pa_sampleFormat, audio_avCodecContext->sample_rate, paFramesPerBufferUnspecified, nullptr, nullptr );
		if (err!=paNoError)
		{
			err = Pa_OpenDefaultStream(&pa_stream, 0, audio_avCodecContext->channels, pa_sampleFormat^paNonInterleaved, audio_avCodecContext->sample_rate, paFramesPerBufferUnspecified, nullptr, nullptr );
			if (err==paNoError)
			{
				//unsigned sampleBytes = Pa_GetSampleSize(pa_sampleFormat); // in one channel
				if (pa_sampleFormat&paNonInterleaved)
				{
					convert_to_interleaved = true;
					//rr::RRReporter::report(rr::INF2,"interleaving, sampleBytes=%d\n",sampleBytes);
				}
				else
				{
					convert_to_planar = true;
					//rr::RRReporter::report(rr::INF2,"deinterleaving, sampleBytes=%d\n",sampleBytes);
				}
			}
			else
			{
				rr::RRReporter::report(rr::WARN,"Pa_OpenDefaultStream()=%d, channels=%d, format=%d->0x%x, rate=%d\n",(int)err,(int)audio_avCodecContext->channels,(int)audio_avCodecContext->sample_fmt,(int)pa_sampleFormat,(int)audio_avCodecContext->sample_rate);
				return -1;
			}
		}

		AVFrame* avFrame = av_frame_alloc();

		while (!aborting)
		{
			if (playing && !pa_started)
			{
				Pa_StartStream(pa_stream);
				pa_started = true;
			}
			else
			if (!playing && pa_started)
			{
				Pa_AbortStream(pa_stream);
				pa_started = false;
			}
			if (pa_started)
			{
				AVPacketWrapper* avPacket = audio_packetQueue.blocking_pop(aborting); // blocking [#62]
				if (avPacket)
				{
					int got_frame = 0;
					avcodec_decode_audio4(audio_avCodecContext, avFrame, &got_frame, avPacket);
					if (got_frame && avFrame->nb_samples)
					{
						unsigned sampleBytes = Pa_GetSampleSize(pa_sampleFormat); // in one channel
						unsigned channelBytes = sampleBytes*avFrame->nb_samples;
						unsigned allBytes = channelBytes*audio_avCodecContext->channels;
						if (buf && allBytes>bufBytes)
							RR_SAFE_DELETE_ARRAY(buf);
						if (convert_to_interleaved)
						{
							if (!buf)
								buf = new char[bufBytes=allBytes];
							char* dst = buf;
							for (unsigned s=0;s<avFrame->nb_samples;s++)
								for (unsigned c=0;c<audio_avCodecContext->channels;c++)
								{
									memcpy(dst,&avFrame->data[c][s*sampleBytes],sampleBytes);
									dst += sampleBytes;
								}
							Pa_WriteStream(pa_stream,buf,avFrame->nb_samples);
						}
						else if (convert_to_planar)
						{
							if (!buf)
								buf = new char[bufBytes=allBytes];
							char* bufPtrs[8] = {buf,buf+channelBytes,buf+2*channelBytes,buf+3*channelBytes,buf+4*channelBytes,buf+5*channelBytes,buf+6*channelBytes,buf+7*channelBytes};
							uint8_t* src = avFrame->data[0];
							for (unsigned s=0;s<avFrame->nb_samples;s++)
								for (unsigned c=0;c<audio_avCodecContext->channels;c++)
								{
									memcpy(&bufPtrs[c][s*sampleBytes],src,sampleBytes);
									src += sampleBytes;
								}
							Pa_WriteStream(pa_stream,bufPtrs,avFrame->nb_samples);
						}
						else
						{
							// very simple, but the only synchronization is blocking when buffer is full,
							// we might need to leave Pa_WriteStream for audio callback later, to improve accuracy
							Pa_WriteStream(pa_stream,avFrame->data,avFrame->nb_samples); // blocking, but only for a short time period, no need for external wake up
						}
					}
					delete avPacket;
				}
				else
				{
					// empty packet = we are either aborting or seeking
					if (!aborting)
						avcodec_flush_buffers(audio_avCodecContext);
				}
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}

		delete[] buf;
		av_frame_free(&avFrame);
		if (pa_started)
			Pa_StopStream(pa_stream);
		Pa_CloseStream(pa_stream);
		Pa_Terminate();
		return 0;
	}


	/////////////////////////////////////////////////////////////////////////
	// video_proc
	/////////////////////////////////////////////////////////////////////////

#ifdef SUPPORT_LIBAV
	// libav
	struct PtsCorrectionContext
	{
		int64_t num_faulty_pts; /// Number of incorrect PTS values so far
		int64_t num_faulty_dts; /// Number of incorrect DTS values so far
		int64_t last_pts;       /// PTS of the last frame
		int64_t last_dts;       /// DTS of the last frame
	};
	void init_pts_correction(PtsCorrectionContext *ctx)
	{
		ctx->num_faulty_pts = ctx->num_faulty_dts = 0;
		ctx->last_pts = ctx->last_dts = INT64_MIN;
	}
	int64_t guess_correct_pts(PtsCorrectionContext *ctx, int64_t reordered_pts, int64_t dts)
	{
		int64_t pts = AV_NOPTS_VALUE;

		if (dts != AV_NOPTS_VALUE)
		{
			ctx->num_faulty_dts += dts <= ctx->last_dts;
			ctx->last_dts = dts;
		}
		if (reordered_pts != AV_NOPTS_VALUE)
		{
			ctx->num_faulty_pts += reordered_pts <= ctx->last_pts;
			ctx->last_pts = reordered_pts;
		}
		if ((ctx->num_faulty_pts<=ctx->num_faulty_dts || dts == AV_NOPTS_VALUE) && reordered_pts != AV_NOPTS_VALUE)
			pts = reordered_pts;
		else
			pts = dts;

		return pts;
	}
#endif

	int video_proc()
	{
		AVFrame* avFrame = NULL;
		double dbg_seekSeconds = -1;
		unsigned dbg_numImagesSinceStart = 0;
		unsigned imagesPushedSinceSeek = 0;
		while (!aborting)
		{
			AVPacketWrapper* avPacket = video_packetQueue.blocking_pop(aborting); // blocking [#61]
			if (aborting)
			{
				delete avPacket;
				break;
			}
			SeekPacket* seekPacket = dynamic_cast<SeekPacket*>(avPacket);
			if (!avPacket || seekPacket)
			{
				// empty packet = new data after seek are coming, we should clean up old data
				avcodec_flush_buffers(video_avCodecContext);
				image_queue.clear(); // remove old images, step 2 (something possibly pushed since step 1)
				imagesPushedSinceSeek = 0;
				dbg_seekSeconds = seekPacket ? seekPacket->seekSeconds : -1;
				delete seekPacket;
				continue;
			}
			int got_frame = 0;
			if (!avFrame)
				avFrame = av_frame_alloc();
			avcodec_decode_video2(video_avCodecContext, avFrame, &got_frame, avPacket);
#ifdef SUPPORT_LIBAV
			int64_t pts = guess_correct_pts(&ptsCorrectionContext,avFrame->pkt_pts,avFrame->pkt_dts);
#else
			int64_t pts = av_frame_get_best_effort_timestamp(avFrame);
#endif
			if (pts==AV_NOPTS_VALUE)
			{
				pts = 0;
			}
			if (got_frame)
			{
				VideoPicture* image_inProgress = new VideoPicture(avFrame, pts * av_q2d(video_avStream->time_base), format);
				RRBuffer* buffer = image_inProgress->buffer;
				if (buffer)
				{
					unsigned bypp = (format==rr::BF_RGB)?3:((format==rr::BF_RGBA)?4:1);
					unsigned char* data = buffer->lock(rr::BL_DISCARD_AND_WRITE);
					uint8_t* dst[] = {data+buffer->getWidth()*(buffer->getHeight()-1)*bypp, nullptr};
					int dstStride[] = {(int)bypp*-(int)buffer->getWidth(), 0};
					sws_scale(video_swsContext, (uint8_t const * const *)avFrame->data, avFrame->linesize, 0, video_avCodecContext->height, dst, dstStride);
					buffer->unlock();
					image_inProgress->dbg_seekSeconds = dbg_seekSeconds;
					image_inProgress->dbg_numImagesSinceStart = dbg_numImagesSinceStart;
					image_inProgress->dbg_numImagesSinceSeek = imagesPushedSinceSeek;
					dbg_numImagesSinceStart++;
					imagesPushedSinceSeek++;
					image_queue.blocking_push(image_inProgress,aborting); // blocking [#50]
				}
				else
				{
					// when something goes terribly wrong and we run out of memory, this is the most likely allocation failure
					delete image_inProgress;
					RR_LIMITED_TIMES(1,RRReporter::report(ERRO,"Video playback run out of address space.\n"));
				}
			}
			delete avPacket;
		}
		av_frame_free(&avFrame);
		return 0;
	}


	/////////////////////////////////////////////////////////////////////////
	// demux_proc
	/////////////////////////////////////////////////////////////////////////

	int demux_proc()
	{
		AVFrame* avFrame = av_frame_alloc();
		//open_file();
		demux_working = true;
		while (!aborting)
		{
			// seek
			if (seekSecondsFromStart>=0)
			{
				demux_working = true;
				audio_packetQueue.clear();
				video_packetQueue.clear();
				image_queue.clear(); // remove old images, step 1 (if video_proc is blocked in blocking_push, this unblocks it)
				if (hasAudio())
					audio_packetQueue.push(nullptr); // avcodec_flush_buffers()
				if (hasVideo())
					video_packetQueue.push(new SeekPacket(seekSecondsFromStart)); // avcodec_flush_buffers() + proper clearing of image_ready and image_inProgress
				RR_DEBUG(int err =) av_seek_frame(avFormatContext, -1, (int64_t)(RR_MAX(0,seekSecondsFromStart-SEEK_SECONDS_BACK) * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD); //|AVSEEK_FLAG_ANY
				RR_ASSERT(err >= 0); // seek failed
				seekSecondsFromStart = -1;
			}

			if (std::min(hasVideo()?video_packetQueue.size():1000,hasAudio()?audio_packetQueue.size():1000)>MAX_PACKET_QUEUE_LENGTH || !demux_working)
			{
				// queues are full, sleep
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			else
			{
				// read packet
				AVPacketWrapper* avPacket = new AVPacketWrapper;
				int err = av_read_frame(avFormatContext, avPacket);
				if (err < 0)
				{
					delete avPacket;
					if (err==AVERROR_EOF) // ffplay tests also avio_feof(avFormatContext->pb), but notes that it's hack
					{
						// this makes demux_proc sleep until main thread requests seek
						demux_working = false;
					}
				}
				else
				{
					// send packet to other thread
					if (avPacket->stream_index == video_streamIndex)
						video_packetQueue.push(avPacket);
					else
					if (avPacket->stream_index == audio_streamIndex)
						audio_packetQueue.push(avPacket);
					else
						delete avPacket;
				}
			}
		}
		av_frame_free(&avFrame);
		demux_working = false;
		return 0;
	}


	/////////////////////////////////////////////////////////////////////////
	// open_file
	/////////////////////////////////////////////////////////////////////////

	static PaSampleFormat convert(AVSampleFormat format)
	{
		switch (format)
		{
			case AV_SAMPLE_FMT_U8: return paUInt8;
			case AV_SAMPLE_FMT_S16: return paInt16;
			case AV_SAMPLE_FMT_S32: return paInt32;
			case AV_SAMPLE_FMT_FLT: return paFloat32;
			case AV_SAMPLE_FMT_U8P: return paUInt8|paNonInterleaved;
			case AV_SAMPLE_FMT_S16P: return paInt16|paNonInterleaved;
			case AV_SAMPLE_FMT_S32P: return paInt32|paNonInterleaved;
			case AV_SAMPLE_FMT_FLTP: return paFloat32|paNonInterleaved;
			default: return 0;
		}
	}

	AVCodecContext* open_stream(int stream_index)
	{
		if (stream_index < 0 || stream_index >= (int)avFormatContext->nb_streams)
			return nullptr;
		AVCodec* avCodec = avcodec_find_decoder(avFormatContext->streams[stream_index]->codec->codec_id);
		if (!avCodec)
		{
			RRReporter::report(WARN,LIB_NAME ": unsupported codec\n");
			return nullptr;
		}
		AVCodecContext* avCodecContext = avcodec_alloc_context3(avCodec);
		int err = avcodec_copy_context(avCodecContext, avFormatContext->streams[stream_index]->codec);
		if (err)
		{
			#define AV_ERROR_MAX_STRING_SIZE 64
			//#define REPORT(func) RRReporter::report(WARN,LIB_NAME ": " func "()=%c%c%c%c\n",char(-err),char((-err)>>8),char((-err)>>16),char((-err)>>24))
			#define REPORT(func,err) { \
				char tmp[AV_ERROR_MAX_STRING_SIZE]; \
				av_strerror(err,tmp,AV_ERROR_MAX_STRING_SIZE); \
				RRReporter::report(WARN,LIB_NAME ": " func "()=%s\n",tmp); \
			}			
			REPORT("avcodec_copy_context",err);
			return nullptr;
		}
		err = avcodec_open2(avCodecContext, avCodec, NULL);
		if (err)
		{
			REPORT("avcodec_open2",err);
			return nullptr;
		}
		return avCodecContext;
	}

	// fills width,height,duration etc, but not image_xxx
	bool open_file(const RRString& filename)
	{
#ifdef SUPPORT_LIBAV
		init_pts_correction(&ptsCorrectionContext);
#endif

		// Open video file (fill avFormatContext)
		int err = avformat_open_input(&avFormatContext, filename.c_str(), NULL, NULL);
		if (err)
		{
			REPORT("avformat_open_input",err);
			return false;
		}

		// Retrieve stream information (fill avFormatContext->streams)
		err = avformat_find_stream_info(avFormatContext, NULL);
		if (err<0) // only negative is error, libav returns 80 on success
		{
			REPORT("avformat_find_stream_info",err);
			return false;
		}

		// some sources claim that duration is filled after avformat_open_input,
		// but experiments show it is not filled before avformat_find_stream_info
		duration = (float)avFormatContext->duration / (float)AV_TIME_BASE;
		// in case of need, duration_estimation_method tells how accurate it is

		// Dump information about file onto standard error
		//av_dump_format(avFormatContext, 0, filename.c_str(), 0);

		// Open streams
		for (unsigned i=0; i<avFormatContext->nb_streams; i++)
		{
			if (audio_streamIndex==-1 && avFormatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO)
			{
				audio_streamIndex = i;
				audio_avStream = avFormatContext->streams[audio_streamIndex];
				if (audio_avStream)
					audio_avCodecContext = open_stream(audio_streamIndex);
				if (audio_avCodecContext)
				{
					audio_thread = std::thread(&FFmpegPlayer::audio_proc,this);
				}
			}
			if (video_streamIndex==-1 && avFormatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
			{
				video_streamIndex = i;
				video_avStream = avFormatContext->streams[video_streamIndex];
				if (video_avStream)
					video_avCodecContext = open_stream(video_streamIndex);
				if (video_avCodecContext)
				{
					const AVPixFmtDescriptor* fmt_desc = av_pix_fmt_desc_get(video_avCodecContext->pix_fmt);
					unsigned idx = RR_CLAMPED(fmt_desc->nb_components,1,4);
					rr::RRBufferFormat rrFormats[5] = {rr::BF_LUMINANCE,rr::BF_LUMINANCE,rr::BF_RGB,rr::BF_RGB,rr::BF_RGBA};
					AVPixelFormat avFormats[5] = {AV_PIX_FMT_GRAY8,AV_PIX_FMT_GRAY8,AV_PIX_FMT_RGB24,AV_PIX_FMT_RGB24,AV_PIX_FMT_RGBA};
					AVPixelFormat avFormat = avFormats[idx];
					video_swsContext = sws_getContext(video_avCodecContext->width, video_avCodecContext->height, video_avCodecContext->pix_fmt, video_avCodecContext->width, video_avCodecContext->height, avFormat, SWS_BILINEAR, NULL, NULL, NULL );
					width = video_avCodecContext->width;
					height = video_avCodecContext->height;
					format = rrFormats[idx];
					video_thread = std::thread(&FFmpegPlayer::video_proc,this);
				}
			}
		}

		return true;
	}


	/////////////////////////////////////////////////////////////////////////
	// RRBuffer-like video API
	/////////////////////////////////////////////////////////////////////////

	bool update()
	{
		// loop
		//  a) play until both audio and video end
		//     "falling snow" looks wrong, it has 1s longer audio than video, video stops for 1s before looping
		//     if (playing && !demux_working && !audio_packetQueue.size() && !video_packetQueue.size() && !image_queue.size())
		//  b) play until video ends (or audio, in audio only files)
		if (looping && playing && !demux_working && ((!hasVideo() && !audio_packetQueue.size()) || (!video_packetQueue.size() && !image_queue.size())))
		{
			seek(0);
		}
		VideoPicture* image = image_queue.pop_older(playing ? startTime.secondsPassed() : stoppedSecondsFromStart);
		if (image)
		{
			delete image_visible;
			image_visible = image;
		}
		return true;
	}
	void play()
	{
		if (!playing)
		{
			startTime.setNow();
			startTime.addSeconds(-stoppedSecondsFromStart);
			playing = true;
		}
	}
	void pause()
	{
		if (playing)
		{
			stoppedSecondsFromStart = startTime.secondsPassed();
			playing = false;
		}
	}
	void seek(float secondsFromStart)
	{
		if (looping && duration>0)
		{
			secondsFromStart = fmod(secondsFromStart,duration);
		}
		if (playing)
		{
			startTime.setNow();
			startTime.addSeconds(secondsFromStart);
		}
		else
		{
			float jump = secondsFromStart-stoppedSecondsFromStart;
			stoppedSecondsFromStart = secondsFromStart;
			if (jump>=0 && jump<=SKIP_SEEK_SECONDS_FORWARD) return; // do nothing when stopped and seeking little bit forward (heavily used by RL anim capture)
		}
		seekSecondsFromStart = secondsFromStart;
	}

	unsigned          width;                  // set once by ctor
	unsigned          height;                 // set once by ctor
	rr::RRBufferFormat format;                // set once by ctor
	float             duration;               // set once by ctor
	bool              looping;                // set once by ctor
	bool              playing;                // changed by main thread, signal to audio_thread
	RRTime            startTime;              // changed by main thread, only valid when playing. also changed by demux_proc when looping (risky)
	float             stoppedSecondsFromStart;// changed by main thread, only valid when !playing
	float             seekSecondsFromStart;   // changed by main thread, signal to demux_thread
	bool              aborting;               // changed by main thread, signal to all background threads

	// demux
	std::thread       demux_thread;           // set once by ctor, feeds video and audio threads from file
	bool              demux_working;          // set on start, cleared when demux reaches end of stream, set on seek
	AVFormatContext*  avFormatContext;        // set once by ctor

	// ffmpeg audio (producer)
	std::thread       audio_thread;           // set once by ctor, decodes audio packets and feeds portaudio
	int               audio_streamIndex;      // set once by ctor
	AVStream*         audio_avStream;         // set once by ctor
	AVCodecContext*   audio_avCodecContext;   // set once by ctor
	Queue<AVPacketWrapper*> audio_packetQueue;// changed by demux_thread and audio_thread

	// ffmpeg video (producer)
	std::thread       video_thread;           // set once by ctor, decodes video packets and feeds image_ready
	int               video_streamIndex;      // set once by ctor
	AVStream*         video_avStream;         // set once by ctor
	AVCodecContext*   video_avCodecContext;   // set once by ctor
	Queue<AVPacketWrapper*> video_packetQueue;// changed by demux_thread and video_thread
	struct SwsContext*video_swsContext;

#ifdef SUPPORT_LIBAV
	PtsCorrectionContext ptsCorrectionContext;
#endif

	// yuv images (consumer)
	Queue<VideoPicture*> image_queue;
	VideoPicture*     image_visible;          // filled by update(), visible from outside
};


//////////////////////////////////////////////////////////////////////////////
//
// RRBufferFFmpeg

// helper for RRBuffer refcounting
static unsigned char g_classHeader[16];
static size_t g_classHeaderSize = 0;

//! RRBuffer interface on top of FFmpegPlayer.
//! RRBuffer is refcounted via "createReference()" and "delete".
class RRBufferFFmpeg : public RRBuffer
{
public:

	//! First in buffer's lifetime (load from one location).
	static RRBuffer* load(const RRString& _filename, const char* _cubeSideName[6])
	{
		RRBufferFFmpeg* video = new RRBufferFFmpeg(_filename);
		if (!video->player->hasAudio() && !video->player->hasVideo())
			RR_SAFE_DELETE(video);
		return video;
	}

	//! Second in buffer's lifetime.
	void* operator new(std::size_t n)
	{
		return malloc(n);
	};

	//! Third in buffer's lifetime. If constructor fails, 0*0 buffer is created.
	RRBufferFFmpeg(const RRString& _filename)
	{
		refCount = 1;
		filename = _filename; // [#36] exact filename we are trying to open (we don't have a locator -> no attempts to open similar names)
		version = rand();
		player = new FFmpegPlayer(filename);
		buffer = nullptr;
	}

	//! When deleting buffer, this is called first.
	virtual ~RRBufferFFmpeg()
	{
		if (--refCount)
		{
			// skip destructor
			filename._skipDestructor();
			// store instance header before destruction
			// (void*) silences warning, we do bad stuff, but it works well with major compilers
			g_classHeaderSize = RR_MIN(16,(char*)&filename-(char*)this); // filename must be first member variable in RRBuffer
			memcpy(g_classHeader,(void*)this,g_classHeaderSize);
		}
		else
		{
			// delete all
			RR_SAFE_DELETE(player);
		}
	}

	//! When deleting buffer, this is called last.
	void operator delete(void* p, std::size_t n)
	{
		if (p)
		{
			RRBufferFFmpeg* b = (RRBufferFFmpeg*)p;
			if (b->refCount)
			{
				// fix instance after destructor, restore first 4 or 8 bytes from backup
				// (it's not safe to restore from local static RRBufferFFmpeg)
				// (void*) silences warning, we do bad stuff, but it works well with major compilers
				memcpy((void*)b,g_classHeader,g_classHeaderSize);
				// however, if last reference remains, try to delete it from cache
				if (b->refCount==1)
					b->deleteFromCache();
			}
			else
			{
				// delete instance after destructor
				free(p);
			}
		}
	};


	/////////////////////////////////////////////////////////////////////////
	// RRBuffer interface 
	/////////////////////////////////////////////////////////////////////////

	// --------- refcounting ---------

	virtual RRBuffer* createReference() override
	{
		refCount++;
		return this;
	}
	virtual unsigned getReferenceCount() override
	{
		return refCount;
	}

	// --------- type/size/format are fixed ---------

	virtual RRBufferType getType() const override
	{
		return BT_2D_TEXTURE;
	}
	virtual unsigned getWidth() const override
	{
		return player->width;
	}
	virtual unsigned getHeight() const override
	{
		return player->height;
	}
	virtual unsigned getDepth() const override
	{
		return 1;
	}
	virtual RRBufferFormat getFormat() const override
	{
		return player->format;
	}
	virtual bool getScaled() const override
	{
		return true;
	}
	virtual bool reset(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, bool scaled, const unsigned char* data) override
	{
		RRReporter::report(WARN,"reset() has no effect on video buffers.\n");
		return false;
	}

	// --------- element access ---------

	virtual void setElement(unsigned index, const RRVec4& _element, const RRColorSpace* colorSpace) override
	{
		RRReporter::report(WARN,"setElement() has no effect on video buffers.\n");
	}
	virtual RRVec4 getElement(unsigned index, const RRColorSpace* colorSpace) const override
	{
		return buffer ? buffer->getElement(index,colorSpace) : RRVec4(0.5f);
	}
	virtual RRVec4 getElementAtPosition(const RRVec3& position, const RRColorSpace* colorSpace, bool interpolated) const override
	{
		return buffer ? buffer->getElementAtPosition(position,colorSpace,interpolated) : RRVec4(0.5f);
	}
	virtual RRVec4 getElementAtDirection(const RRVec3& direction, const RRColorSpace* colorSpace) const override
	{
		return buffer ? buffer->getElementAtDirection(direction,colorSpace) : RRVec4(0.5f);
	}

	// --------- whole buffer access ---------

	virtual unsigned char* lock(RRBufferLock lock) override
	{
		return buffer ? buffer->lock(lock) : nullptr;
	}
	virtual void unlock() override
	{
	}

	// --------- video access ---------

	virtual bool update() override
	{
		bool result = player->update();
		if (result)
		{
			version++;
			buffer = player->image_visible ? player->image_visible->buffer : nullptr;
		}
		return result;
	}
	virtual void play() override
	{
		player->play();
	}
	virtual void stop() override
	{
		player->pause();
		player->seek(0);
	}
	virtual void pause() override
	{
		player->pause();
	}
	virtual void seek(float secondsFromStart) override
	{
		player->seek(secondsFromStart);
	}
	virtual float getDuration() const override
	{
		return player->duration;
	}
 
private:
	std::atomic<unsigned> refCount;

	FFmpegPlayer* player; // indirection helps when we want player to survive ~RRBufferFFmpeg()
	RRBuffer* buffer; // shortcut for player->image_visible->buffer
};


/////////////////////////////////////////////////////////////////////////////
//
// main

void registerLoaderFFmpegLibav()
{
	// We list only several important fileformats here.
	// If you need FFmpeg to work with different fileformats, modify our list.
	// You can even change it to "*.*".
	RRBuffer::registerLoader("*.avi;*.mkv;*.mov;*.wmv;*.mpg;*.mpeg;*.mp4;*.mp3;*.wav",RRBufferFFmpeg::load);
}

#endif // SUPPORT_FFMPEG || SUPPORT_LIBAV
