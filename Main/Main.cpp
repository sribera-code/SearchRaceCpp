
#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace std::literals::complex_literals;
using Z = std::complex<double>;
using Angle = int;
using Thrust = unsigned;
const double Pi = 3.141592653589793238463;

struct IO
{
	static IO make()
	{
		return { std::cin, std::cerr, std::cout };
	}

	template<typename T>
	T read(bool end = false)
	{
		std::string s;
		m_in >> s;
		m_read << s << " ";
		std::istringstream is(std::move(s));
		T t;
		is >> t;
		if (end)
		{
			m_read << "\\n";
			m_in.ignore();
		}
		return t;
	}

	std::string getLastRead()
	{
		auto lastRead = m_read.str();
		m_read.str("");
		return lastRead;
	}

	std::istream& m_in;
	std::ostream& m_err;
	std::ostream& m_out;
	std::ostringstream m_read;
};

template<>
Z IO::read<Z>(bool end)
{
	return { read<double>(), read<double>(end) };
}

struct Test
{
	Test()
		: m_io({ m_in, std::cerr, m_out })
	{}

	std::istringstream m_in;
	//std::ostringstream m_err;
	std::ostringstream m_out;
	IO m_io;
};


struct Game
{
	static Game read(IO& io)
	{
		Game g;
		std::generate_n(std::back_inserter(g.m_checkpoints), io.read<std::size_t>(true), [&io]() { return io.read<Z>(true); });
		return g;
	}

	std::vector<Z> m_checkpoints;
};

template<typename C, typename D>
static std::ostream& join(std::ostream& os, C const& collection, D const& delimiter)
{
	if (!collection.empty())
		std::for_each(collection.begin() + 1, collection.end(), [&o = os << collection.front(), &delimiter](auto const& v) { o << delimiter << v; });
	return os;
}

static std::ostream& operator<<(std::ostream& os, Game const& g)
{
	os << std::fixed << std::setprecision(0) << "Game: size=" << g.m_checkpoints.size() << " checkpoints=[";
	return join(os, g.m_checkpoints, ", ") << "]";
}

struct State
{
	std::size_t m_step = {};
	Z m_position, m_speed;
	Angle m_angle = {};

	static State read(IO& io)
	{
		return { io.read<std::size_t>(), io.read<Z>(), io.read<Z>(), io.read<Angle>(true) };
	}
};

static std::ostream& operator<<(std::ostream& os, State const& s)
{
	return os << std::fixed << std::setprecision(0) << "State: step=" << s.m_step << " position=" << s.m_position << " speed=" << s.m_speed << " angle=" << s.m_angle;
}

const Angle angleMax = 18;
const Thrust thrustMax = 200;

static void runGame()
{
	auto io = IO::make();
	auto g = Game::read(io);
	io.m_err << io.getLastRead() << std::endl;
	io.m_err << g << std::endl;
	auto seed = static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count());
	std::default_random_engine generator(seed);
	std::uniform_int_distribution<Angle> distributionAngle(-angleMax, +angleMax);
	std::uniform_int_distribution<Thrust> distributionThrust(0, +thrustMax);
	while (true)
	{
		auto s = State::read(io);
		io.m_err << io.getLastRead() << std::endl;
		io.m_err << s << std::endl;
		io.m_out << "EXPERT " << distributionAngle(generator) << " " << distributionThrust(generator) << std::endl;;
	}
}

#ifndef TESTS
int main()
{
	runGame();
}
#endif
