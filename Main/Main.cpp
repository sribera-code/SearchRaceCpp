
const auto testGame = true;
const auto withRandomTests = true;

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <complex>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <cmath> 
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace std::literals::complex_literals;

using Distance = double;
using Norm = double;
using Z = std::complex<Distance>;
using Angle = int;
using Thrust = unsigned;
using Step = std::size_t;
using Milliseconds = unsigned;

const double pi = 3.141592653589793238463;
const double radByDeg = pi / 180.;
const double degByRad = 180. / pi;
const Distance epsilon = .00001;
const double degEpsilon = .1;
const Angle angleMax = 18;
const Thrust thrustMax = 200;
const Distance xMax = 16000, yMax = 9000, checkpointRadius = 600;
const double checkpointRadiusSquare = checkpointRadius * checkpointRadius;
const double friction = 0.15;
const Milliseconds stepTime = 40;
const unsigned iterationsLimit = 600;

template <typename T>
static T getInfinity()
{
	return std::numeric_limits<T>::max();
}

template <typename Destination, typename Source>
static void transfer(Destination& destination, Source&& source)
{
	destination = source;
}

template <typename Destination, typename Source, typename... Args>
static void transfer(Destination& destination, Source&& source, Args&&...args)
{
	destination = source;
	transfer(std::forward<Args>(args)...);
}

using TimePoint = std::chrono::high_resolution_clock::time_point;

static TimePoint now()
{
	return std::chrono::high_resolution_clock::now();
}

static Milliseconds getMillisecondsElapsed(TimePoint const& start, TimePoint const& end)
{
	return static_cast<Milliseconds>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
}

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

static const unsigned getSeed()
{
	//const auto seed = static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count());
	const auto seed = 0u;
	std::cerr << "seed=" << seed << std::endl;
	return seed;
}

static const unsigned seed()
{
	//static const auto seed = static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count());
	static const auto seed = getSeed();
	return seed;
}

template<typename T, T min, T max>
static T getRandom()
{
	static std::default_random_engine generator(seed());
	//static std::default_random_engine generator(0u);
	static std::uniform_int_distribution<T> distribution(min, max);
	return distribution(generator);
}

static auto getRandomAngle = getRandom<Angle, -angleMax, +angleMax>;
static auto getRandomThrust = getRandom<Thrust, 0, thrustMax>;

template<typename T, T tMin, T tMax>
static T getAngle(T t)
{
	if (t < tMin) return getAngle<T, tMin, tMax>(t + tMax - tMin);
	if (t < tMax) return t;
	return getAngle<T, tMin, tMax>(t - tMax + tMin);
}

template<typename T>
static T get360Angle(T t)
{
	return getAngle<T, 0, 360>(t);
}

template<typename T>
static T get180Angle(T t)
{
	return getAngle<T, -180, +180>(t);
}

template<typename T, T tMin, T tMax>
static T getValid(T t)
{
	if (t < tMin) return tMin;
	if (t > tMax) return tMax;
	return t;
}

template<typename T, T tMin, T tMax>
static bool isValid(T t)
{
	return tMin <= t && t <= tMax;
}

static auto getValidAngle = getValid<Angle, -angleMax, +angleMax>;
static auto getValidThrust = getValid<Thrust, 0, thrustMax>;

static auto isValidAngle = isValid<Angle, -angleMax, +angleMax>;
static auto isValidThrust = isValid<Thrust, 0, thrustMax>;

static std::array<Z, 360> getPolarArray()
{
	std::array<Z, 360> polarArray;
	for (Angle angle = 0; angle < 360; ++angle)
		polarArray[angle] = std::polar(1., radByDeg * angle);
	return polarArray;
}

static Z getPolar(Angle angle)
{
	static auto const polarArray = getPolarArray();
	return polarArray[get360Angle(angle)];
}

static Z truncateZ(Z z)
{
	return { std::trunc(z.real() + copysign(epsilon, z.real())), std::trunc(z.imag() + copysign(epsilon, z.imag())) };
}

struct Game
{
	static Game read(IO& io)
	{
		Game g;
		std::generate_n(std::back_inserter(g.m_checkpoints), io.read<Step>(true), [&io]() { return io.read<Z>(true); });
		return g;
	}

	Z getCheckpoint(Step step) const
	{
		return m_checkpoints[step];
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
	State() = default;
	State(Step step, Z position, Z speed, Angle angle)
		: m_step(step), m_position(std::move(position)), m_speed(std::move(speed)), m_angle(get360Angle(angle))
	{}

	Step m_step = {};
	Z m_position, m_speed;
	Angle m_angle = {};

	static State read(IO& io)
	{
		return { io.read<Step>(), io.read<Z>(), io.read<Z>(), io.read<Angle>(true) };
	}
};

static bool operator==(State const& lhs, State const& rhs)
{
	std::cerr << (lhs.m_step == rhs.m_step) << (lhs.m_angle == rhs.m_angle)
		<< (std::norm(lhs.m_position - rhs.m_position) < epsilon)
		<< (std::norm(lhs.m_speed - rhs.m_speed) < epsilon) << std::endl;
	return lhs.m_step == rhs.m_step && lhs.m_angle == rhs.m_angle
		&& std::norm(lhs.m_position - rhs.m_position) < epsilon
		&& std::norm(lhs.m_speed - rhs.m_speed) < epsilon;
}

static std::ostream& operator<<(std::ostream& os, State const& s)
{
	return os << std::fixed << std::setprecision(0) << "State: step=" << s.m_step
		<< " position=" << s.m_position << "[" << std::abs(s.m_position) << "," << std::arg(s.m_position) * degByRad << "deg]"
		<< " speed=" << s.m_speed << "[" << std::abs(s.m_speed) << "," << std::arg(s.m_speed) * degByRad << "deg] angle=" << s.m_angle << "deg";
}

struct Command
{
	Command() = default;
	Command(Angle angle, Thrust thrust) : m_angle(angle), m_thrust(thrust) {}

	static Command makeValidCommand(Angle angle, Thrust thrust)
	{
		return { getValidAngle(get180Angle(angle)), getValidThrust(thrust) };
	}

	Angle m_angle = {};
	Thrust m_thrust = {};

	static Command getRandom()
	{
		return { getRandomAngle(), getRandomThrust() };
	}

	State move(State state) const
	{
		state.m_angle = get360Angle(state.m_angle + m_angle);
		state.m_speed += static_cast<Z::value_type>(m_thrust) * getPolar(state.m_angle);
		state.m_position += state.m_speed;
		state.m_speed *= 1. - friction;
		return state;
	}

	State move(Game const& game, State state) const
	{
		auto oldPosition = state.m_position;
		state = move(std::move(state));
		auto newPosition = state.m_position;
		auto checkpoint = game.getCheckpoint(state.m_step);
		if (std::norm(checkpoint - newPosition) <= checkpointRadiusSquare)
			++state.m_step;
		else
		{
			auto deltaPosition = newPosition - oldPosition;
			auto distance = std::abs(deltaPosition);
			auto product = deltaPosition * std::conj(checkpoint - oldPosition) / distance;
			if (-checkpointRadius <= product.imag() && product.imag() <= +checkpointRadius)
				if (0 <= product.real() && product.real() <= distance)
					++state.m_step;
		}
		state.m_position = truncateZ(state.m_position);
		state.m_speed = truncateZ(state.m_speed);
		return state;
	}
};

static Command getDirectCommand(Game const& game, State const& currentState)
{
	auto computeStatus = [&game](State const& state) { return std::make_tuple(-static_cast<int>(state.m_step), std::norm(game.getCheckpoint(state.m_step) - state.m_position)); };
	auto bestStatus = std::make_tuple(0, getInfinity<Norm>());
	Command bestCommand;
	for (Angle angle = -angleMax; angle <= angleMax; ++angle)
	{
		Command command(angle, thrustMax);
		auto newState = command.move(game, currentState);
		auto newStatus = computeStatus(newState);
		if (newStatus < bestStatus)
			transfer(bestStatus, newStatus, bestCommand, command);
	}
	Command commandWithoutThrust(bestCommand.m_angle, 0);
	auto newStateWithoutThrust = commandWithoutThrust.move(game, currentState);
	auto newStatusWithoutThrust = computeStatus(newStateWithoutThrust);
	if (newStatusWithoutThrust <= bestStatus)
		return commandWithoutThrust;
	auto currentStatus = computeStatus(currentState);
	if (currentStatus <= bestStatus)
		return commandWithoutThrust;
	return bestCommand;

	//auto target = game.getCheckpoint(currentState.m_step);
	//auto distanceToTarget = std::abs(positionToTarget);
	//auto angleRadius = std::atan2(checkpointRadius, distanceToTarget);
	//auto angleSpeed = std::arg(state.m_speed) * degByRad;
	//auto angleToTarget = std::arg(positionToTarget) * degByRad;
	//auto commandAngle = get180Angle(static_cast<Angle>(std::round(angleToTarget - state.m_angle)));
	//if (isValidAngle(commandAngle))
	//	return Command(commandAngle, thrustMax);
	//return Command(getValidAngle(commandAngle), 0);
}

static std::ostream& operator<<(std::ostream& os, Command const& c)
{
	return os << "EXPERT " << c.m_angle << " " << c.m_thrust << std::endl;
}

static unsigned reachNext(Game const& game, TimePoint const& start, unsigned timeLimit, Step initialStep, State state)
{
	auto step = std::min(initialStep + 2, game.m_checkpoints.size() - 1);
	unsigned time = 0;
	while (state.m_step < step)
	{
		if (++time >= timeLimit)
			return getInfinity<unsigned>();
		auto command = getDirectCommand(game, state);
		state = command.move(game, state);
		if (getMillisecondsElapsed(start, now()) >= stepTime)
			return getInfinity<unsigned>();
	}
	return time;
}

static unsigned runGame(IO& io, Game const& game)
{
	State lastState;
	unsigned totalTestsCount = 0, totalImprovementsCount = 0, iterationsCount = 0;
	while (true)
	{
		auto currentState = State::read(io);
		io.m_err << io.getLastRead() << std::endl;
		io.m_err << "old: " << currentState << std::endl;
		if (!iterationsCount)
			io.m_err << io.getLastRead() << std::endl;
		else if (testGame)
			assert(lastState == currentState);
		++iterationsCount;

		auto bestCommand = getDirectCommand(game, currentState);
		auto bestState = bestCommand.move(game, currentState);
		auto start = now();
		auto bestTime = withRandomTests ? reachNext(game, start, getInfinity<unsigned>(), currentState.m_step, bestState) : 0;
		unsigned testsCount = 0;
		if (withRandomTests)
			while (getMillisecondsElapsed(start, now()) < stepTime)
			{
				++testsCount;
				auto command = Command::getRandom();
				auto state = command.move(game, currentState);
				auto time = reachNext(game, start, bestTime, currentState.m_step, state);
				if (time < bestTime)
				{
					transfer(bestTime, time, bestCommand, command, bestState, state);
					++totalImprovementsCount;
					io.m_err << "best: time=" << bestTime << " state=" << bestState << std::endl;
				}
			}
		bestState = bestCommand.move(game, currentState);
		io.m_err << "elapsed=" << getMillisecondsElapsed(start, now()) << "ms" << std::endl;
		io.m_err << "testsCount=" << testsCount << " totalImprovements=" << totalImprovementsCount << std::endl;
		totalTestsCount += testsCount;
		io.m_err << "best: time=" << bestTime << " - " << bestState << std::endl;
		bool endGame = bestState.m_step == game.m_checkpoints.size() || iterationsCount == iterationsLimit;
		if (endGame)
			io.m_err << "totalTestsCount=" << totalTestsCount << " iterationsCount=" << iterationsCount << std::endl;
		lastState = bestState;
		io.m_out << bestCommand;
		if (endGame)
			break;
	}
	return iterationsCount;
}

static unsigned runGame()
{
	auto io = IO::make();
	const auto game = Game::read(io);
	io.m_err << io.getLastRead() << std::endl;
	io.m_err << game << std::endl;
	return runGame(io, game);
}

#ifndef TESTS
int main()
{
	runGame();
}
#endif
