#include "sha1.h"
#include <time.h>

#define EXTRACT_EXPIRATION_DAY \
	sha1::sha1_context ctx; \
	uint8 digest[20]; \
	sha1::sha1_starts(&ctx); \
	sha1::sha1_update(&ctx,(unsigned char*)licenseOwner,100); \
	sha1::sha1_update(&ctx,(unsigned char*)licenseNumber,100); \
	sha1::sha1_finish(&ctx,digest); \
	unsigned expDay = 5536*digest[15]+197*digest[1]+digest[11]; \
	time_t expTime = expDay*24*3600; \
	time_t nowTime = time(NULL); \
	unsigned nowDay = (unsigned)(nowTime/(24*3600));
