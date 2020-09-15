#include <chrono>

enum class RunLevel { Debug = 0, Test = 1, PreValidation = 2, Validation = 3, Release = 4 };

struct Config
{
	bool m_withRandomTests = true;
	//bool m_withRandomTests = false;
	bool m_simulation = false;
	std::chrono::milliseconds m_turnTime = std::chrono::milliseconds(40);
	//RunLevel m_runLevel = RunLevel::Debug;
	RunLevel m_runLevel = RunLevel::Release;

	unsigned m_testSequencesSizeMax = 3;
	unsigned m_testSequenceIterationsMax = 5;
	unsigned m_targetStep = 2;
	double m_speedFactor = 3.;
};

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <deque>
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

#define doAtLevel(game, runLevel) if (game.m_config.m_runLevel <= runLevel)
#define logAtLevel(game, runLevel, io) doAtLevel(game, runLevel) io.m_err
#define assertAtLevel(game, runLevel, expression) doAtLevel(game, runLevel) assert(expression)

using namespace std::literals::complex_literals;

using Distance = double;
using Norm = double;
using Z = std::complex<Distance>;
using Angle = int;
using Thrust = unsigned;
using Step = std::size_t;
using Count = unsigned;
using Iteration = unsigned;
using Milliseconds = unsigned;
using Index = std::size_t;

const double pi = 3.141592653589793238463;
const double radByDeg = pi / 180.;
const double degByRad = 180. / pi;
const Distance epsilon = .00001;
const Angle angleMax = 18;
const Thrust thrustMax = 200;
const Distance xMax = 16000, yMax = 9000, checkpointRadius = 600;
const double checkpointRadiusSquare = checkpointRadius * checkpointRadius;
const double friction = 0.15;
const Iteration iterationLimit = 600;
const std::chrono::milliseconds firstTurnTime(1000);
const std::chrono::milliseconds turnTime(50);

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
	destination = std::forward<Source>(source);
	transfer(std::forward<Args>(args)...);
}

using TimePoint = std::chrono::steady_clock::time_point;

static TimePoint now()
{
	return std::chrono::steady_clock::now();
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

static const unsigned seed()
{
	//static const auto seed = static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count());
	static const auto seed = 0u;
	return seed;
}

template<typename T>
static T getRandom(T min, T max)
{
	static std::default_random_engine generator(seed());
	static std::uniform_int_distribution<T> distribution;
	return min + distribution(generator) % (max - min + 1);
}

template<typename T, T min, T max>
static T getRandom()
{
	static std::default_random_engine generator(seed());
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

struct Checkpoints
{
	std::vector<Z> m_checkpoints;
	std::vector<Distance> m_distances;
	std::vector<Step> m_targetSteps;

	static Checkpoints read(IO& io, Config const& config)
	{
		Checkpoints checkpoints;

		std::generate_n(std::back_inserter(checkpoints.m_checkpoints), io.read<Step>(true), [&io]() { return io.read<Z>(true); });

		checkpoints.m_distances.resize(checkpoints.m_checkpoints.size() - 1, 0.);
		for (Index index = 0; index < checkpoints.m_distances.size(); ++index)
			checkpoints.m_distances[index] = std::abs(checkpoints.m_checkpoints[index + 1] - checkpoints.m_checkpoints[index]);
		
		checkpoints.m_targetSteps.resize(checkpoints.m_checkpoints.size(), 0u);
		for (Step step = 0; step < checkpoints.m_targetSteps.size(); ++step)
		{
			auto targetStep = step + config.m_targetStep;
			if (targetStep - 1 < checkpoints.m_distances.size() && checkpoints.m_distances[targetStep - 1] < 4000)
				++targetStep;
			checkpoints.m_targetSteps[step] = std::min(targetStep, checkpoints.m_checkpoints.size());
		}

		return checkpoints;
	}
};

struct Game
{
	Config m_config;
	Checkpoints m_checkpoints;
};

template<typename C, typename D>
static std::ostream& join(std::ostream& os, C const& collection, D const& delimiter)
{
	if (!collection.empty())
		std::for_each(collection.begin() + 1, collection.end(), [&o = os << collection.front(), &delimiter](auto const& v) { o << delimiter << v; });
	return os;
}

static std::ostream& operator<<(std::ostream& os, Checkpoints const& g)
{
	os << std::fixed << std::setprecision(0) << "Checkpoints: size=" << g.m_checkpoints.size() << " checkpoints=[";
	return join(os, g.m_checkpoints, ", ") << "]";
}

struct State
{
	State() = default;
	State(Step step, Z position, Z speed, Angle angle)
		: m_step(step), m_position(std::move(position)), m_speed(std::move(speed)), m_angle(get360Angle(angle))
	{}

	Step m_step = {};
	double m_collisionTime = {};
	Iteration m_iteration = {};
	Z m_position, m_speed;
	Angle m_angle = {};

	static State read(IO& io)
	{
		return { io.read<Step>(), io.read<Z>(), io.read<Z>(), io.read<Angle>(true) };
	}
};

static bool operator==(State const& lhs, State const& rhs)
{
	return lhs.m_step == rhs.m_step && lhs.m_iteration == rhs.m_iteration && lhs.m_angle == rhs.m_angle
		&& std::norm(lhs.m_position - rhs.m_position) < epsilon
		&& std::norm(lhs.m_speed - rhs.m_speed) < epsilon;
}

static bool operator!=(State const& lhs, State const& rhs)
{
	return !(lhs == rhs);
}

static void logDifference(Game const& game, IO& io, State const& lhs, State const& rhs)
{
	logAtLevel(game, RunLevel::Debug, io)
	    << "Diff: step=" << (lhs.m_step == rhs.m_step) << " iteration=" << (lhs.m_iteration == rhs.m_iteration) << " angle=" << (lhs.m_angle == rhs.m_angle)
		<< " position=" << (std::norm(lhs.m_position - rhs.m_position) < epsilon)
		<< " speed=" << (std::norm(lhs.m_speed - rhs.m_speed) < epsilon) << std::endl;
}

static std::ostream& operator<<(std::ostream& os, State const& state)
{
	return os << std::fixed << std::setprecision(0) << "State: step=" << state.m_step << " collisionTime=" << state.m_collisionTime * 100 << "% iteration=" << state.m_iteration
		<< " position=" << state.m_position << "[" << std::abs(state.m_position) << "," << std::arg(state.m_position) * degByRad << "deg]"
		<< " speed=" << state.m_speed << "[" << std::abs(state.m_speed) << "," << std::arg(state.m_speed) * degByRad << "deg] angle=" << state.m_angle << "deg";
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

	static double getCollisionTime(Z const& position, Z const& speed, Z const& checkpoint)
	{
		auto x = position.real() - checkpoint.real();
		auto y = position.imag() - checkpoint.imag();
		auto vx = speed.real();
		auto vy = speed.imag();

		auto a = vx * vx + vy * vy;
		auto b = 2. * (x * vx + y * vy);
		auto c = x * x + y * y - checkpointRadiusSquare;
		auto delta = b * b - 4. * a * c;

		return delta < 0. ? -1 : (-b - sqrt(delta)) / (2. * a);
	}

	State move(Game const& game, State state) const
	{
		auto const& checkpoint = game.m_checkpoints.m_checkpoints[state.m_step];

		state.m_angle = get360Angle(state.m_angle + m_angle);
		state.m_speed += static_cast<Z::value_type>(m_thrust) * getPolar(state.m_angle);
		state.m_collisionTime = getCollisionTime(state.m_position, state.m_speed, checkpoint);
		state.m_position += state.m_speed;
		state.m_speed *= 1. - friction;
		++state.m_iteration;

		if (0 <= state.m_collisionTime && state.m_collisionTime <= 1.)
			++state.m_step;
		else
			state.m_collisionTime = 0.;

		state.m_position = truncateZ(state.m_position);
		state.m_speed = truncateZ(state.m_speed);
		return state;
	}
};

static std::ostream& operator<<(std::ostream& os, Command const& c)
{
	return os << "EXPERT " << c.m_angle << " " << c.m_thrust;
}

static Command getDirectCommand(Game const& game, IO& io, State const& state)
{
	auto nexTarget = game.m_checkpoints.m_checkpoints[state.m_step] - state.m_position - game.m_config.m_speedFactor * state.m_speed;
	auto angleSpeed = std::arg(state.m_speed) * degByRad;
	auto angleToTarget = std::arg(nexTarget) * degByRad;
	auto commandAngle = get180Angle(static_cast<Angle>(std::round(angleToTarget - state.m_angle)));
	if (isValidAngle(commandAngle))
		return Command(commandAngle, thrustMax);
	return Command(getValidAngle(commandAngle), 0);
}

struct TestSequence
{
	enum class Type { Direct = 0, Left = 1, Right = 2, Count = 3 };

	Type m_type = Type::Direct;
	bool m_thrust = true;
	Count m_iterations = {};
};

static std::ostream& operator<<(std::ostream& os, TestSequence const& testSequence)
{
	if (testSequence.m_type == TestSequence::Type::Direct)
		os << "D";
	else if (testSequence.m_type == TestSequence::Type::Left)
		os << "L";
	else // if (testSequence.m_type == TestSequence::Type::Right)
		os << "R";
	if (testSequence.m_thrust)
		os << "T";
	return os << testSequence.m_iterations;
}

static bool operator==(TestSequence const& lhs, TestSequence const& rhs)
{
	return lhs.m_type == rhs.m_type && lhs.m_thrust == rhs.m_thrust && lhs.m_iterations == rhs.m_iterations;
}

const auto lastTestSequenceType = static_cast<int>(TestSequence::Type::Count) - 1;

template<typename T, T min, T max>
static T getRandomExcept(T except)
{
	auto random = getRandom<T, min, max - 1>();
	return random >= except ? random + 1 : random;
}

static bool getRandomBool()
{
	return !getRandom<unsigned, 0, 1>();
}

using TestSequences = std::deque<TestSequence>;

static std::ostream& operator<<(std::ostream& os, TestSequences const& testSequences)
{
	for (auto const& testSequence : testSequences)
		os << testSequence;
	return os;
}

static TestSequence getRandomTestSequence(Game const& game, bool last)
{
	TestSequence testSequence;
	auto type = last ? getRandomExcept<int, 0, lastTestSequenceType>(static_cast<int>(TestSequence::Type::Direct)) : getRandom<int, 0, lastTestSequenceType>();
	testSequence.m_type = static_cast<TestSequence::Type>(type);
	testSequence.m_thrust = testSequence.m_type == TestSequence::Type::Direct || getRandomBool();
	testSequence.m_iterations = getRandom<Count>(1, game.m_config.m_testSequenceIterationsMax);
	return testSequence;
}

static TestSequence getRandomTestSequence(Game const& game, bool last, TestSequence* previous, TestSequence* next)
{
	auto random = getRandomTestSequence(game, last);
	if (previous && *previous == random)
		return getRandomTestSequence(game, last, previous, next);
	if (next && *next == random)
		return getRandomTestSequence(game, last, previous, next);
	return random;
}

static TestSequences getRandomTestSequences(Game const& game)
{
	Count size = getRandom<Count>(1, game.m_config.m_testSequencesSizeMax);
	TestSequences testSequences(size);
	for (unsigned test = 0; test < size; ++test)
	{
		testSequences[test] = getRandomTestSequence(game, test + 1 == size, test ? &testSequences[test-1] : nullptr, nullptr);
	}
	return testSequences;
}

static TestSequences mutateTestSequences(Game const& game, TestSequences testSequences)
{
	if (getRandomBool())
		for (int index = 0; index < static_cast<int>(testSequences.size()); ++index)
		{
			testSequences[index].m_iterations += getRandom<int, -1, +1>();
			if (!testSequences[index].m_iterations)
			{
				testSequences.erase(testSequences.begin() + index);
				--index;
			}
		}
	if (getRandomBool())
		for (unsigned index = 0; index < testSequences.size(); ++index)
		{
			if (!getRandom<std::size_t>(0, testSequences.size()))
			{
				testSequences.insert(testSequences.begin() + index, getRandomTestSequence(game, index == testSequences.size() - 1, index ? &testSequences[index-1] : nullptr, &testSequences[index]));
			}
		}
	if (getRandomBool())
		testSequences.push_back(getRandomTestSequence(game, true, testSequences.empty() ? nullptr : &testSequences.back(), nullptr));
	return testSequences;
}

static Command popCommand(TestSequences& testSequences, Game const& game, IO& io, State state)
{
	if (testSequences.empty())
		return getDirectCommand(game, io, state);
	Command command;
	auto& testSequence = testSequences.front();
	if (testSequence.m_type == TestSequence::Type::Direct)
		command = getDirectCommand(game, io, state);
	else
	{
		if (testSequence.m_type == TestSequence::Type::Left)
			command.m_angle = +angleMax;
		else // if (testSequence.m_type == TestSequence::Type::Right)
			command.m_angle = -angleMax;
		if (testSequence.m_thrust)
			command.m_thrust = thrustMax;
	}
	if (!--testSequence.m_iterations)
		testSequences.pop_front();
	return command;
}

struct StepIteration
{
	Step m_step = 0;
	Iteration m_iteration = 0;
	double m_collisionTime = 0.;
};

static bool operator<(StepIteration const& lhs, StepIteration const& rhs)
{
	if (lhs.m_step != rhs.m_step)
		return lhs.m_step > rhs.m_step;
	if (lhs.m_iteration != rhs.m_iteration)
		return lhs.m_iteration < rhs.m_iteration;
	return lhs.m_collisionTime < rhs.m_collisionTime;
}

static bool operator==(StepIteration const& lhs, StepIteration const& rhs)
{
	return lhs.m_step == rhs.m_step && lhs.m_iteration == rhs.m_iteration && lhs.m_collisionTime == rhs.m_collisionTime;
}

static std::ostream& operator<<(std::ostream& os, StepIteration const& iteration)
{
	return os << "step=" << iteration.m_step << " iteration=" << iteration.m_iteration << " collisionTime=" << 100 * iteration.m_collisionTime << "%";
}

static StepIteration reachNext(IO& io, Game const& game, StepIteration const& stepIterationMax, Step targetStep, State state, TestSequences testSequences)
{
	Iteration iterationMax = targetStep == stepIterationMax.m_step ? stepIterationMax.m_iteration : iterationLimit;
	while (true)
	{
		if (state.m_step >= targetStep)
			return { state.m_step, state.m_iteration, state.m_collisionTime };
		if (state.m_iteration >= iterationMax)
			return { 0, iterationLimit, 0. };
		auto command = popCommand(testSequences, game, io, state);
		state = command.move(game, std::move(state));
	}
}

struct Result
{
	Count m_gamesCount = 0u;
	Count m_iterationsCount = 0u;
	Milliseconds m_elpased = 0;
	Count m_testsCount = 0u;
	Count m_randomImprovementsCount = 0u;
	Count m_mutationImprovementsCount = 0u;
};

static Result operator+(Result const& lhs, Result const& rhs)
{
	return { lhs.m_gamesCount + rhs.m_gamesCount, lhs.m_iterationsCount + rhs.m_iterationsCount, lhs.m_elpased + rhs.m_elpased,
		lhs.m_testsCount + rhs.m_testsCount, lhs.m_randomImprovementsCount + rhs.m_randomImprovementsCount, lhs.m_mutationImprovementsCount + rhs.m_mutationImprovementsCount };
}

static std::ostream& operator<<(std::ostream& os, Result const& result)
{
	return os << "gamesCount=" << result.m_gamesCount << " averageIterationsCount=" << (result.m_iterationsCount / result.m_gamesCount)
		<< " averageElapsed=" << (result.m_elpased / result.m_iterationsCount)
		<< " averageTestsCount=" << (result.m_testsCount / result.m_iterationsCount)
		<< " averageRandomImprovementsCount=" << ((100 * result.m_randomImprovementsCount) / result.m_iterationsCount) << "%"
		<< " averageMutationImprovementsCount=" << ((100 * result.m_mutationImprovementsCount) / result.m_iterationsCount) << "%";
}

static Result runGame(Config const& config, IO& io)
{
	auto startTimepoint = now();
	Game game;
	game.m_config = config;
	logAtLevel(game, RunLevel::Test, io) << "seed=" << seed << std::endl;
	game.m_checkpoints = Checkpoints::read(io, game.m_config);
	auto timepoint = now();
	logAtLevel(game, RunLevel::Debug, io) << io.getLastRead() << std::endl;
	logAtLevel(game, RunLevel::Test, io) << game.m_checkpoints << std::endl;
	State lastState;
	Result result;
	result.m_gamesCount = 1;

	TestSequences bestTestSequences;
	StepIteration bestIteration;

	while (true)
	{
		auto currentState = game.m_config.m_simulation && result.m_iterationsCount ? lastState : State::read(io);
		currentState.m_iteration = result.m_iterationsCount;
		logAtLevel(game, RunLevel::Debug, io) << io.getLastRead() << std::endl;
		logAtLevel(game, RunLevel::Test, io) << "old: " << currentState << std::endl;
		doAtLevel(game, RunLevel::Validation)
			if (result.m_iterationsCount && lastState != currentState)
			{
				logDifference(game, io, lastState, currentState);
				assertAtLevel(game, RunLevel::Validation, false);
			}
		++result.m_iterationsCount;
		auto targetStep = game.m_checkpoints.m_targetSteps[currentState.m_step];

		Command bestCommand;
		State bestState;

		logAtLevel(game, RunLevel::Debug, io) << "step=" << currentState.m_step << " targetStep=" << targetStep << std::endl;
		Count testsCount = 0;
		if (game.m_config.m_withRandomTests)
		{
			auto replaceBest = [&](StepIteration iteration, TestSequences testSequences)
			{
				auto command = popCommand(testSequences, game, io, currentState);
				auto state = command.move(game, currentState);
				transfer(bestIteration, std::move(iteration), bestCommand, std::move(command), bestState, std::move(state), bestTestSequences, std::move(testSequences));
				logAtLevel(game, RunLevel::Debug, io) << "bestIteration: " << bestIteration << " bestState: " << bestState << std::endl;
			};
			{
				++testsCount;
				auto iteration = reachNext(io, game, bestIteration, targetStep, currentState, bestTestSequences);
				logAtLevel(game, RunLevel::Test, io) << "iteration: " << iteration << " bestIteration: " << bestIteration << std::endl;
				assertAtLevel(game, RunLevel::Validation, iteration.m_step != bestIteration.m_step || iteration == bestIteration);
				if (iteration < bestIteration || iteration == bestIteration)
				{
					replaceBest(iteration, bestTestSequences);
				}
			}
			if (!bestTestSequences.empty())
			{
				++testsCount;
				TestSequences testSequences;
				auto iteration = reachNext(io, game, bestIteration, targetStep, currentState, testSequences);
				if (iteration < bestIteration)
				{
					replaceBest(iteration, std::move(testSequences));
				}
			}
			auto initialTestSequences = bestTestSequences;
			while (now() - timepoint < game.m_config.m_turnTime)
			{
				{
					++testsCount;
					auto testSequences = mutateTestSequences(game, initialTestSequences);
					auto iteration = reachNext(io, game, bestIteration, targetStep, currentState, testSequences);
					if (iteration < bestIteration)
					{
						++result.m_randomImprovementsCount;
						logAtLevel(game, RunLevel::Debug, io) << "mutation ";
						replaceBest(iteration, std::move(testSequences));
					}
				}
				{
					++testsCount;
					auto testSequences = getRandomTestSequences(game);
					auto iteration = reachNext(io, game, bestIteration, targetStep, currentState, testSequences);
					if (iteration < bestIteration)
					{
						++result.m_mutationImprovementsCount;
						logAtLevel(game, RunLevel::Debug, io) << "random ";
						replaceBest(iteration, std::move(testSequences));
					}
				}
			}
		}
		else
		{
			bestCommand = getDirectCommand(game, io, currentState);
			bestState = bestCommand.move(game, currentState);
		}
		logAtLevel(game, RunLevel::Test, io) << "testsCount=" << testsCount << " totalRandomImprovements=" << result.m_randomImprovementsCount << " totalMutationImprovements=" << result.m_mutationImprovementsCount
			<< std::endl << " bestIteration: " << bestIteration << " bestCommand: " << bestCommand << " bestTestSequences: " << bestTestSequences << std::endl;
		result.m_testsCount += testsCount;
		logAtLevel(game, RunLevel::Test, io)<< "bestState: " << bestState << std::endl;
		bool endGame = bestState.m_step == game.m_checkpoints.m_checkpoints.size() || result.m_iterationsCount == iterationLimit;
		if (endGame)
		{
			result.m_elpased = getMillisecondsElapsed(startTimepoint, now());
			logAtLevel(game, RunLevel::PreValidation, io) << result << std::endl;
		}
		lastState = bestState;
		io.m_out << bestCommand << std::endl;
		logAtLevel(game, RunLevel::Test, io) << "elapsed=" << getMillisecondsElapsed(timepoint, now()) << "ms" << std::endl;
		//assertAtLevel(game, RunLevel::Debug, now() - timepoint <= (result.m_iterationsCount <= 1 ? firstTurnTime : turnTime));
		timepoint = now();
		if (endGame)
			break;
	}
	return result;
}

static void runGame()
{
	Config config;
	auto io = IO::make();
	runGame(config, io);
}

#ifndef TESTS
int main()
{
	runGame();
}
#endif
