/*

	NoZ Game Engine

	Copyright(c) 2025 NoZ Games, LLC

*/

namespace noz::msdf
{
	struct SignedDistance
	{
		static SignedDistance Infinite;

		double distance;
		double dot;

		SignedDistance(double distance, double dot);

		bool operator< (const SignedDistance& rhs)
		{
			return std::abs(distance) < std::abs(rhs.distance) || (std::abs(distance) == std::abs(rhs.distance) && dot < rhs.dot);
		}

		bool operator> (const SignedDistance& rhs)
		{
			return std::abs(distance) > std::abs(rhs.distance) || (std::abs(distance) == std::abs(rhs.distance) && dot > rhs.dot);
		}

		bool operator <= (const SignedDistance& rhs)
		{
			return std::abs(distance) < std::abs(rhs.distance) || (std::abs(distance) == std::abs(rhs.distance) && dot <= rhs.dot);
		}

		bool operator >=(const SignedDistance& rhs)
		{
			return std::abs(distance) > std::abs(rhs.distance) || (std::abs(distance) == std::abs(rhs.distance) && dot >= rhs.dot);
		}
	};
}
