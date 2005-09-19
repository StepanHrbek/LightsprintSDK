#ifndef COLLIDER_SHA1_H
#define COLLIDER_SHA1_H

#ifndef PRIVATE
	#define PRIVATE
#endif

namespace sha1
{

	#ifndef uint8
	#define uint8  unsigned char
	#endif

	#ifndef uint32
	#define uint32 unsigned long int
	#endif

	struct sha1_context
	{
		uint32 total[2];
		uint32 state[5];
		uint8 buffer[64];
	};

	PRIVATE void sha1_starts( sha1_context *ctx );
	PRIVATE void sha1_update( sha1_context *ctx, uint8 *input, uint32 length );
	PRIVATE void sha1_finish( sha1_context *ctx, uint8 digest[20] );

};

#endif
