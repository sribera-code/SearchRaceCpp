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
	auto g = Game::read(m_io);
	m_io.m_err << g << std::endl;
	EXPECT_EQ(replaceSub(m_io.getLastRead(), "\\\\n", "\n"), gameInput);
}
