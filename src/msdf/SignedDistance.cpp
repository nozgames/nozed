/*

	NoZ Game Engine

	Copyright(c) 2025 NoZ Games, LLC

*/

#include "SignedDistance.h"

namespace noz::msdf
{
	SignedDistance SignedDistance::Infinite = SignedDistance(-100000.0, 1.0);

	SignedDistance::SignedDistance(double distance, double dot)
	{
		this->distance = distance;
		this->dot = dot;
	}
}
