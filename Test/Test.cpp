#include "pch.h"
#include <cassert>
#include <regex>

#define TESTS
#include "../Main/Main.cpp"

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

struct SearchRaceTest : public ::testing::Test
{
	SearchRaceTest() : m_io({ m_in, std::cerr, m_out }) {}

	std::istringstream m_in;
	std::ostringstream m_out;
	IO m_io;
};

TEST_F(SearchRaceTest, ReadGameInput)
{
	std::string gameInput = "9 \n2757 4659 \n3358 2838 \n10353 1986 \n2757 4659 \n3358 2838 \n10353 1986 \n2757 4659 \n3358 2838 \n10353 1986 \n";
	m_in.str(gameInput);
	auto game = Game::read(m_io);
	EXPECT_EQ(replaceSub(m_io.getLastRead(), "\\\\n", "\n"), gameInput);
	EXPECT_EQ(game.m_checkpoints.size(), 9);
	EXPECT_EQ(game.getCheckpoint(3), Z(2757, 4659));
	std::string stateInput = "0 10353 1986 0 0 161 \n";
	m_in.str(stateInput);
	auto state = State::read(m_io);
	EXPECT_EQ(replaceSub(m_io.getLastRead(), "\\\\n", "\n"), stateInput);
	EXPECT_EQ(state.m_step, 0);
	EXPECT_EQ(state.m_position, Z(10353, 1986));
	EXPECT_EQ(state.m_speed, Z(0, 0));
	EXPECT_EQ(state.m_angle, 161);
}

TEST_F(SearchRaceTest, WriteCommand)
{
	EXPECT_EQ(toString(Command{ angleMax, thrustMax }), "EXPERT 18 200\n");
	EXPECT_EQ(toString(Command()), "EXPERT 0 0\n");
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

TEST_F(SearchRaceTest, State)
{
	State state;
	state.m_angle = 140;
	Command command(10, 100);
	auto newState = command.move(state);
	EXPECT_EQ(state.m_angle + command.m_angle, newState.m_angle);
	EXPECT_NEAR(std::arg(newState.m_speed) * degByRad, newState.m_angle, degEpsilon);
}

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
	std::string gameInput = "9 \n2757 4659 \n3358 2838 \n10353 1986 \n2757 4659 \n3358 2838 \n10353 1986 \n2757 4659 \n3358 2838 \n10353 1986 \n";
	m_in.str(gameInput);
	auto game = Game::read(m_io);
}
