#include "pch.h"
#include <atomic>
#include <regex>
#include <thread>

#define TESTS
#include "../Main/Main.cpp"

const double degEpsilon = .1;

static std::string replaceSub(std::string const& str, std::string const& sub, std::string const& rep)
{
	std::string res;
	std::regex_replace(std::back_inserter(res), str.begin(), str.end(), std::regex(sub), rep);
	return res;
}

template<typename T>
static std::string toString(T&& t)
{
	std::ostringstream s; s << t;
	return s.str();
}

struct GameInput
{
	std::string m_label;
	std::string m_checkpoints;
	std::string m_initialState;
};

struct TestIO
{
	TestIO() : m_io({ m_in, std::cerr, m_out })
	{}

	TestIO(TestIO&& other) noexcept
		: m_in(std::move(other.m_in))
		, m_out(std::move(other.m_out))
		, m_io({ m_in, std::cerr, m_out })
	{}

	std::istringstream m_in;
	std::ostringstream m_out;
	IO m_io;
};

struct SearchRaceTest : public ::testing::Test
{
	SearchRaceTest()
	{
		m_config.m_simulation = true;
		m_config.m_runLevel = RunLevel::Validation;

		//m_config.m_withRandomTests = false;
		//m_config.m_runLevel = RunLevel::Test;
		//m_config.m_runLevel = RunLevel::Debug;
		//m_testParameters = true;

		m_maxThreadsCount = 3u;
		m_runsCount = 2u;

		if (m_config.m_runLevel < RunLevel::Validation || !m_config.m_withRandomTests)
		{
			m_maxThreadsCount = 1u;
			m_runsCount = 1u;
		}
	}

	Config m_config;
	Count m_maxThreadsCount = 0, m_runsCount = 0;
	bool m_testParameters = false;

	Result runGame(TestIO& io, GameInput const& input)
	{
		io.m_in.str(input.m_checkpoints + input.m_initialState);
		return ::runGame(m_config, io.m_io);
	}

	static void simulateGame(SearchRaceTest* test, TestIO* io, GameInput const& input, Result* result)
	{
		for (unsigned run = 0u; run < test->m_runsCount; ++run)
			*result = *result + test->runGame(*io, input);
	}

	struct RunInput
	{
		RunInput(SearchRaceTest& test, GameInput const& input) : m_test(test), m_input(input), m_done(false) {}

		RunInput(RunInput&&) = default;
		RunInput& operator=(RunInput&&) = default;

		SearchRaceTest& m_test;
		TestIO m_io;
		GameInput const& m_input;
		Result m_result;
		bool m_done;
	};

	static void simulateGames(std::vector<RunInput>* runInputs, std::atomic<unsigned>* atomicIndex)
	{
		auto next = [&]() { return atomicIndex->fetch_add(1); };
		auto index = next();
		while (index < runInputs->size())
		{
			auto& runInput = (*runInputs)[index];
			simulateGame(&runInput.m_test, &runInput.m_io, runInput.m_input, &runInput.m_result);
			runInput.m_done = true;
			index = next();
		}
	}

	static void displayResults(std::vector<RunInput>* runInputs, bool intermediaryResults)
	{
		Result result;
		for (auto const& runInput : *runInputs)
		{
			while (!runInput.m_done)
				std::this_thread::yield();
			if (intermediaryResults)
				runInput.m_io.m_io.m_err << "Test(" << runInput.m_input.m_label << "): " << runInput.m_result << std::endl;
			EXPECT_LE(runInput.m_result.m_elpased, firstStepTime.count() + (runInput.m_result.m_iterationsCount - 1) * stepTime.count());
			EXPECT_LT((runInput.m_result.m_iterationsCount / runInput.m_result.m_gamesCount), iterationLimit);
			result = result + runInput.m_result;
		}

		TestIO io;
		if (intermediaryResults)
			io.m_io.m_err << "------ " << std::endl;
		io.m_io.m_err << "Tests: " << result << std::endl;
		EXPECT_LE(result.m_elpased, firstStepTime.count() + (result.m_iterationsCount - 1) * stepTime.count());
		EXPECT_LT((result.m_iterationsCount / result.m_gamesCount), iterationLimit);
	}

	void runGames(std::vector<GameInput> const& inputs, bool intermediaryResults = true)
	{
		std::vector<RunInput> runInputs;
		for (auto const& input : inputs)
			runInputs.emplace_back(*this, input);

		std::atomic<unsigned> atomicIndex = 0u;
		std::vector<std::thread> threads(m_maxThreadsCount <= 1 ? 0 : m_maxThreadsCount);
		if (threads.empty())
			simulateGames(&runInputs, &atomicIndex);
		else
			for (auto& thread : threads)
				thread = std::thread(simulateGames, &runInputs, &atomicIndex);

		if (threads.empty())
			displayResults(&runInputs, intermediaryResults);
		else
			threads.push_back(std::thread(displayResults, &runInputs, intermediaryResults));
		for (auto& thread : threads)
			if (thread.joinable())
				thread.join();
	}

	void testParameters(std::vector<GameInput> const& inputs)
	{
		TestIO io;
		io.m_io.m_err << "------ " << std::endl;
		//for (m_config.m_testSequencesSizeMax = 2u; m_config.m_testSequencesSizeMax <= 4; ++m_config.m_testSequencesSizeMax)
		{
			//for (m_config.m_testSequenceIterationsMax = 4u; m_config.m_testSequenceIterationsMax <= 6u; m_config.m_testSequenceIterationsMax += 1)
			{
				//for (m_config.m_targetStep = 2u; m_config.m_targetStep <= 4u; m_config.m_targetStep += 1)
				{
					for (m_config.m_speedFactor = 1.; m_config.m_speedFactor <= 6.; m_config.m_speedFactor += .1)
					{
						//for (int useDisksOfRotation = 1; useDisksOfRotation >= 0; m_config.m_useDisksOfRotation = !!--useDisksOfRotation)
						{
							//for (m_config.m_targetDistance = 0.; m_config.m_targetDistance <= 10000.; m_config.m_targetDistance += 1000.)
							{
								io.m_io.m_err << std::fixed << std::setprecision(2) << "testSequenceIterationsMax=" << m_config.m_testSequenceIterationsMax << " testSequencesSizeMax=" << m_config.m_testSequencesSizeMax << " targetStep=" << m_config.m_targetStep
									<< " speedFactor=" << std::setprecision(2) << m_config.m_speedFactor << " useDisksOfRotation=" << m_config.m_useDisksOfRotation << " targetDistance=" << m_config.m_targetDistance << std::endl;
								runGames(inputs, false);
								io.m_io.m_err << "------ " << std::endl;
							}
						}
					}
				}
			}
		}
	}
};

TEST_F(SearchRaceTest, ReadGameInput)
{
	TestIO io;
	std::string gameInput = "9 \n2757 4659 \n3358 2838 \n10353 1986 \n2757 4659 \n3358 2838 \n10353 1986 \n2757 4659 \n3358 2838 \n10353 1986 \n";
	io.m_in.str(gameInput);
	auto checkpoints = Checkpoints::read(io.m_io, m_config);
	EXPECT_EQ(replaceSub(io.m_io.getLastRead(), "\\\\n", "\n"), gameInput);
	EXPECT_EQ(checkpoints.m_checkpoints.size(), 9);
	EXPECT_EQ(checkpoints.m_checkpoints[3], Z(2757, 4659));
	std::string stateInput = "0 10353 1986 0 0 161 \n";
	io.m_in.str(stateInput);
	auto state = State::read(io.m_io);
	EXPECT_EQ(replaceSub(io.m_io.getLastRead(), "\\\\n", "\n"), stateInput);
	EXPECT_EQ(state.m_step, 0);
	EXPECT_EQ(state.m_position, Z(10353, 1986));
	EXPECT_EQ(state.m_speed, Z(0, 0));
	EXPECT_EQ(state.m_angle, 161);
}

TEST_F(SearchRaceTest, WriteCommand)
{
	EXPECT_EQ(toString(Command{ angleMax, thrustMax }), "EXPERT 18 200");
	EXPECT_EQ(toString(Command()), "EXPERT 0 0");
}

TEST_F(SearchRaceTest, RandomCommand)
{
	for (unsigned i = 0; i < 100; ++i)
	{
		auto c = Command::getRandom();
		EXPECT_LE(std::abs(c.m_angle), angleMax);
		EXPECT_LE(c.m_thrust, thrustMax);
	}
}

TEST_F(SearchRaceTest, Polar)
{
	EXPECT_LE(std::abs(getPolar(- 90) - (-1.i)), epsilon);
	EXPECT_LE(std::abs(getPolar(   0) - (+1. )), epsilon);
	EXPECT_LE(std::abs(getPolar(+ 90) - (+1.i)), epsilon);
	EXPECT_LE(std::abs(getPolar(+180) - (-1. )), epsilon);
	EXPECT_LE(std::abs(getPolar(+270) - (-1.i)), epsilon);
	EXPECT_LE(std::abs(getPolar(+360) - (+1. )), epsilon);
	EXPECT_LE(std::abs(getPolar(+450) - (+1.i)), epsilon);
}

#if 0
TEST_F(SearchRaceTest, State)
{
	State state;
	state.m_angle = 140;
	Command command(10, 100);
	auto newState = command.move(state);
	EXPECT_EQ(state.m_angle + command.m_angle, newState.m_angle);
	EXPECT_NEAR(std::arg(newState.m_speed) * degByRad, newState.m_angle, degEpsilon);
}
#endif

TEST_F(SearchRaceTest, Transfer)
{
	unsigned u = 3u;
	std::string s = "3";
	double d = -3.;
	transfer(u, 2u, s, "2", d, -2.);
	EXPECT_EQ(u, 2u);
	EXPECT_EQ(s, "2");
	EXPECT_EQ(d, -2.);
}

TEST_F(SearchRaceTest, Simulations)
{
	std::vector<GameInput> const& inputs = {
		{ "1", "9 \n2757 4659 \n3358 2838 \n10353 1986 \n2757 4659 \n3358 2838 \n10353 1986 \n2757 4659 \n3358 2838 \n10353 1986 \n"
		, "0 10353 1986 0 0 161 \n" },
		{ "2", "9 \n3431 6328 \n4284 2801 \n11141 4590 \n3431 6328 \n4284 2801 \n11141 4590 \n3431 6328 \n4284 2801 \n11141 4590 \n"
		, "0 11141 4590 0 0 167 \n" },
		{ "3", "21 \n10892 5399 \n4058 1092 \n6112 2872 \n1961 6027 \n7148 4594 \n7994 1062 \n1711 3942 \n10892 5399 \n4058 1092 \n6112 2872 \n1961 6027 \n7148 4594 \n7994 1062 \n1711 3942 \n10892 5399 \n4058 1092 \n6112 2872 \n1961 6027 \n7148 4594 \n7994 1062 \n1711 3942 \n"
		, "0 1711 3942 0 0 9 \n" },
		{ "4", "24 \n1043 1446 \n10158 1241 \n13789 7502 \n7456 3627 \n6218 1993 \n7117 6546 \n5163 7350 \n12603 1090 \n1043 1446 \n10158 1241 \n13789 7502 \n7456 3627 \n6218 1993 \n7117 6546 \n5163 7350 \n12603 1090 \n1043 1446 \n10158 1241 \n13789 7502 \n7456 3627 \n6218 1993 \n7117 6546 \n5163 7350 \n12603 1090 \n"
		, "0 12603 1090 0 0 178 \n" },
		{ "5", "24 \n1271 7171 \n14407 3329 \n10949 2136 \n2443 4165 \n5665 6432 \n3079 1942 \n4019 5141 \n9214 6145 \n1271 7171 \n14407 3329 \n10949 2136 \n2443 4165 \n5665 6432 \n3079 1942 \n4019 5141 \n9214 6145 \n1271 7171 \n14407 3329 \n10949 2136 \n2443 4165 \n5665 6432 \n3079 1942 \n4019 5141 \n9214 6145 \n"
		, "0 9214 6145 0 0 173 \n" },
		{ "6", "24 \n11727 5704 \n11009 3026 \n10111 1169 \n5835 7503 \n1380 2538 \n4716 1269 \n4025 5146 \n8179 7909 \n11727 5704 \n11009 3026 \n10111 1169 \n5835 7503 \n1380 2538 \n4716 1269 \n4025 5146 \n8179 7909 \n11727 5704 \n11009 3026 \n10111 1169 \n5835 7503 \n1380 2538 \n4716 1269 \n4025 5146 \n8179 7909 \n"
		, "0 8179 7909 0 0 328 \n" },
		{ "7", "24 \n14908 1849 \n2485 3249 \n5533 6258 \n12561 1063 \n1589 6883 \n13542 2666 \n13967 6917 \n6910 1656 \n14908 1849 \n2485 3249 \n5533 6258 \n12561 1063 \n1589 6883 \n13542 2666 \n13967 6917 \n6910 1656 \n14908 1849 \n2485 3249 \n5533 6258 \n12561 1063 \n1589 6883 \n13542 2666 \n13967 6917 \n6910 1656 \n"
		, "0 6910 1656 0 0 1 \n" },
		{ "8", "24 \n9882 5377 \n3692 3080 \n3562 1207 \n4231 7534 \n14823 6471 \n10974 1853 \n9374 3740 \n4912 4817 \n9882 5377 \n3692 3080 \n3562 1207 \n4231 7534 \n14823 6471 \n10974 1853 \n9374 3740 \n4912 4817 \n9882 5377 \n3692 3080 \n3562 1207 \n4231 7534 \n14823 6471 \n10974 1853 \n9374 3740 \n4912 4817 \n"
		, "0 4912 4817 0 0 6 \n" },
		{ "9", "24 \n1271 7171 \n14407 3329 \n10949 2136 \n2443 4165 \n5665 6432 \n3079 1942 \n4019 5141 \n9214 6145 \n1271 7171 \n14407 3329 \n10949 2136 \n2443 4165 \n5665 6432 \n3079 1942 \n4019 5141 \n9214 6145 \n1271 7171 \n14407 3329 \n10949 2136 \n2443 4165 \n5665 6432 \n3079 1942 \n4019 5141 \n9214 6145 \n"
		, "0 9214 6145 0 0 173 \n" },
		{ "10", "24 \n9623 7597 \n12512 6231 \n4927 3377 \n8358 6630 \n4459 7216 \n10301 2326 \n2145 3943 \n5674 4795 \n9623 7597 \n12512 6231 \n4927 3377 \n8358 6630 \n4459 7216 \n10301 2326 \n2145 3943 \n5674 4795 \n9623 7597 \n12512 6231 \n4927 3377 \n8358 6630 \n4459 7216 \n10301 2326 \n2145 3943 \n5674 4795 \n"
		, "0 5674 4795 0 0 35 \n" },
		{ "11", "24 \n14203 4266 \n3186 5112 \n8012 5958 \n2554 6642 \n5870 4648 \n11089 2403 \n9144 2389 \n12271 7160 \n14203 4266 \n3186 5112 \n8012 5958 \n2554 6642 \n5870 4648 \n11089 2403 \n9144 2389 \n12271 7160 \n14203 4266 \n3186 5112 \n8012 5958 \n2554 6642 \n5870 4648 \n11089 2403 \n9144 2389 \n12271 7160 \n"
		, "0 12271 7160 0 0 304 \n" },
		{ "12", "24 \n1779 2501 \n5391 2200 \n13348 4290 \n6144 4176 \n11687 5637 \n14990 3490 \n3569 7566 \n14086 1366 \n1779 2501 \n5391 2200 \n13348 4290 \n6144 4176 \n11687 5637 \n14990 3490 \n3569 7566 \n14086 1366 \n1779 2501 \n5391 2200 \n13348 4290 \n6144 4176 \n11687 5637 \n14990 3490 \n3569 7566 \n14086 1366 \n"
		, "0 14086 1366 0 0 175 \n" },
		{ "13", "24 \n6419 7692 \n2099 4297 \n13329 3186 \n13870 7169 \n13469 1115 \n5176 5061 \n1260 7235 \n9302 5289 \n6419 7692 \n2099 4297 \n13329 3186 \n13870 7169 \n13469 1115 \n5176 5061 \n1260 7235 \n9302 5289 \n6419 7692 \n2099 4297 \n13329 3186 \n13870 7169 \n13469 1115 \n5176 5061 \n1260 7235 \n9302 5289 \n"
		, "0 9302 5289 0 0 140 \n" },
		{ "14", "24 \n10177 7892 \n5146 7584 \n11531 1216 \n1596 5797 \n8306 3554 \n5814 2529 \n9471 5505 \n6752 5734 \n10177 7892 \n5146 7584 \n11531 1216 \n1596 5797 \n8306 3554 \n5814 2529 \n9471 5505 \n6752 5734 \n10177 7892 \n5146 7584 \n11531 1216 \n1596 5797 \n8306 3554 \n5814 2529 \n9471 5505 \n6752 5734 \n"
		, "0 6752 5734 0 0 32 \n" },
		{ "15", "24 \n10312 1696 \n2902 6897 \n5072 7852 \n5918 1004 \n3176 2282 \n14227 2261 \n9986 5567 \n9476 3253 \n10312 1696 \n2902 6897 \n5072 7852 \n5918 1004 \n3176 2282 \n14227 2261 \n9986 5567 \n9476 3253 \n10312 1696 \n2902 6897 \n5072 7852 \n5918 1004 \n3176 2282 \n14227 2261 \n9986 5567 \n9476 3253 \n"
		, "0 9476 3253 0 0 298 \n" },
		{ "16", "18 \n12000 1000 \n12500 2500 \n13000 4000 \n12500 5500 \n12000 7000 \n1000 1000 \n12000 1000 \n12500 2500 \n13000 4000 \n12500 5500 \n12000 7000 \n1000 1000 \n12000 1000 \n12500 2500 \n13000 4000 \n12500 5500 \n12000 7000 \n1000 1000 \n"
		, "0 1000 1000 0 0 0 \n" },
		{ "17", "24 \n12500 2500 \n12500 5500 \n12000 7000 \n8000 7000 \n7500 5500 \n7500 2500 \n8000 1000 \n12000 1000 \n12500 2500 \n12500 5500 \n12000 7000 \n8000 7000 \n7500 5500 \n7500 2500 \n8000 1000 \n12000 1000 \n12500 2500 \n12500 5500 \n12000 7000 \n8000 7000 \n7500 5500 \n7500 2500 \n8000 1000 \n12000 1000 \n"
		, "0 12000 1000 0 0 72 \n" },
		{ "18", "24 \n2500 3905 \n4000 5095 \n5500 3905 \n7000 5095 \n8500 3905 \n10000 5095 \n11500 3905 \n1000 4500 \n2500 3905 \n4000 5095 \n5500 3905 \n7000 5095 \n8500 3905 \n10000 5095 \n11500 3905 \n1000 4500 \n2500 3905 \n4000 5095 \n5500 3905 \n7000 5095 \n8500 3905 \n10000 5095 \n11500 3905 \n1000 4500 \n"
		, "0 1000 4500 0 0 338 \n" },
		{ "19", "18 \n15000 8000 \n1000 8000 \n15000 1000 \n1000 4500 \n15000 4500 \n1000 1000 \n15000 8000 \n1000 8000 \n15000 1000 \n1000 4500 \n15000 4500 \n1000 1000 \n15000 8000 \n1000 8000 \n15000 1000 \n1000 4500 \n15000 4500 \n1000 1000 \n"
		, "0 1000 1000 0 0 27 \n" }
	};
	if (m_testParameters)
		testParameters(inputs);
	else
		runGames(inputs);
}


