#include "Monitors.h"
#include "AutoSDL.h"
#include "LogDef.h"
#include "NewDef.h"
namespace Auto3D {

Monitors::Monitors()
{
	SDL_Rect rect;
	_monitorCount = SDL_GetNumVideoDisplays();
	for (int i = 0; i < _monitorCount; i++)
	{
		SDL_GetDisplayBounds(i, &rect);
		_mnitors.push_back(MakePair(rect.w, rect.h));
	}
}

int Monitors::GetMonitorsCount()
{
	return _monitorCount;
}

int Monitors::GetMonitorsHeight(int index)
{
	if (index > _monitorCount - 1)
	{
		WarningString("Fail to get monitors with index(Maybe index beyond the limit)");
		return 0;
	}
	return _mnitors.at(index).second;
}

int Monitors::GetMonitorsWidth(int index)
{
	if (index > _monitorCount - 1)
	{
		WarningString("Fail to get monitors with index(Maybe index beyond the limit)");
		return 0;
	}
	return _mnitors.at(index).first;
}

RectInt Monitors::GetMonitorsSize(int index)
{
	if (index > _monitorCount - 1)
	{
		WarningString("Fail to get monitors size index(Maybe index beyond the limit)");
	}
	RectInt rectSize;
	rectSize._right = _mnitors.at(index).first + rectSize._left;
	rectSize._top = _mnitors.at(index).second +rectSize._bottom;
	return rectSize;
}

}