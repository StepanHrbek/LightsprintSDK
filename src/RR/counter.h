#ifndef __Counter_H__
#define __Counter_H__

#include <iostream>

class Counter
{
	int pos, lastpos, size;
	bool done;
	const int CONSOLE_WIDTH;
public:
	Counter(int _size) : CONSOLE_WIDTH(78)
	{
		Reset(_size);
	}
	~Counter()
	{
		if (!done) End();
	}
	void Next()
	{
		if (size == 0) return;
		if (pos > 0)
		{
			int n = pos*CONSOLE_WIDTH/size;
			int i;
			if (lastpos < n)
			{
				for (i = lastpos + 1; i <= n; i++) std::cout << "-";
				std::cout.flush();
				lastpos = n;
			}
		}
		else
		{
			std::cout << "<";
			std::cout.flush();
		}
		pos++;
	}
	void Reset(int _size)
	{
		pos = 0;
		lastpos = 0;
		size = _size;
		done = false;
	}

	void Counter::End()
	{
		if (size)
		{
			int i;
			for (i = lastpos + 1; i <= CONSOLE_WIDTH; i++) std::cout << "-";
			std::cout << ">" << std::endl;
		}
		Reset(0);
		done = true;
	}
};

#endif //__Counter_H__
