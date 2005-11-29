#include <stdio.h>
#include <string.h>

#define MAX_SCENES 20

struct Item
{
	int rays;
	int memory;
	char* scan(char* pos)
	{
		if(sscanf(pos,"%5d/%03d",&rays,&memory)!=2)
		{
			//if(pos && *pos && *pos!='\n') printf("Invalid expresion: '%s'",pos);
			return NULL;
		}
		return pos+9;
	}
};

class Line
{
public:
	Line()
	{
		numItems = 0;
	}
	void scan(char* line)
	{
		while(line)
		{
			line = items[numItems].scan(line);
			if(line) numItems++;
		}
	}
	void compareAgainst(Line* ref, float* raysChg, float* memChg)
	{
		if(ref->numItems!=numItems)
		{
			puts("Comparing different line lengths, skip.");
			return;
		}
		float raysChgSum = 0;
		float memChgSum = 0;
		for(unsigned i=0;i<numItems;i++)
		{
			float raysChg1 = (items[i].rays-ref->items[i].rays)/((float)ref->items[i].rays)*100;
			float memChg1 = (items[i].memory-ref->items[i].memory)/((float)ref->items[i].memory)*100;
			raysChgSum += raysChg1;
			memChgSum += memChg1;
			//printf(" %+.1f/%+.1f",raysChg1,memChg1);
		}
		*raysChg = raysChgSum/numItems;
		*memChg = memChgSum/numItems;
		printf(" avg=%+.1f%c/%+.1f%c\n",*raysChg,'%',*memChg,'%');
	}

private:
	unsigned numItems;
	Item items[MAX_SCENES];
};

int main(int argc, char** argv)
{
	if(argc!=3)
	{
err:
		puts("usage: bench_report bench_last_filename bench_all_filename");
		return 1;
	}

	// read fall
	FILE* fall = fopen(argv[2],"rt");
	if(!fall) goto err;
	const int MAX_LINE_LEN = 1000;
	char line[MAX_LINE_LEN];
	Line allLines[100];
	unsigned numLines = 0;
	while(fgets(line,MAX_LINE_LEN,fall) && strlen(line)>5)
	{
		allLines[numLines++].scan(line);
	}
	fclose(fall);

	// read flast
	FILE* flast = fopen(argv[1],"rt");
	if(!flast) goto err;
	fgets(line,MAX_LINE_LEN,flast);
	allLines[numLines].scan(line);
	fclose(flast);

	// report
	if(numLines>0)
	{
		float raysChg = 0;
		float memChg = 0;
		allLines[numLines].compareAgainst(&allLines[numLines-1],&raysChg,&memChg);
	}
	else
	{
		puts("");
	}

	return 0;
}
