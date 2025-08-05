#include "tests.p4_depot_path.h"

#include "tests.common.h"
#include "utils/p4_depot_path.h"

int TestP4DepotPath()
{
	TEST_START();

	const char* pPathStr = "//Depot/Path/To/file";
	P4DepotPath p(pPathStr);
	TEST(p.AsString(), pPathStr);
	TEST(p.GetDepotName(), "Depot");
	TEST(p.GetParts(), std::vector<P4DepotPath::Part>({ "Depot", "Path", "To", "file" }));

	const char* pPathMixedCase = "//Depot/PATH/to/FiLe";
	P4DepotPath pMixedCase(pPathMixedCase);
	TEST(pMixedCase, p);
	TEST(p.GetParts(), pMixedCase.GetParts());
	TEST_NEQ(pMixedCase.AsString(), p.AsString());

	const char* pPathWithSlashStr = "//Depot/Branch/2/";
	P4DepotPath pWSlash(pPathWithSlashStr);
	TEST(pWSlash.AsString(), pPathWithSlashStr);
	TEST(pWSlash.GetDepotName(), "Depot");
	TEST(pWSlash.GetParts(), std::vector<P4DepotPath::Part>({ "Depot", "Branch", "2" }));

	P4DepotPath pathWOEndingSlash("//Depot/Branch/2");
	P4DepotPath pathWEndingSlash("//Depot/Branch/2/");
	TEST(pathWOEndingSlash, pathWEndingSlash);

	{
		bool threw = false;
		try
		{
			P4DepotPath p("something");
		}
		catch (const P4DepotPath::InvalidDepotPathException& e)
		{
			threw = true;
		}

		TEST(threw, true);

		threw = false;

		try
		{
			P4DepotPath p("/some/path/to/file.txt");
		}
		catch (const P4DepotPath::InvalidDepotPathException& e)
		{
			threw = true;
		}

		TEST(threw, true);

		threw = false;

		try
		{
			P4DepotPath p("");
		}
		catch (const P4DepotPath::InvalidDepotPathException& e)
		{
			threw = true;
		}

		TEST(threw, true);
	}

	P4DepotPath::Part part1 = "Depot";
	P4DepotPath::Part part1B = "depot";
	P4DepotPath::Part part2 = "Branch";
	P4DepotPath::Part part2B = "BRaNcH";
	TEST(part1, part1B);
	TEST(part2, part2B);

	auto pParts = p.GetParts();
	P4DepotPath pFromParts(pParts.begin(), pParts.end());
	TEST(p, pFromParts);

	P4DepotPath pSplicedFull = p.Splice(0, 3);
	TEST(p, pSplicedFull);

	P4DepotPath pSplicedPartial = p.Splice(1, 2);
	TEST_NEQ(p, pSplicedPartial);
	P4DepotPath pPartial("//Path/To");
	TEST(pSplicedPartial, pPartial);

	{
		bool threw = false;
		try
		{
			p.Splice(0, 135);
		}
		catch (const std::out_of_range& e)
		{
			threw = true;
		}

		TEST(threw, true);

		threw = false;

		try
		{
			p.Splice(2, 1);
		}
		catch (const std::invalid_argument& e)
		{
			threw = true;
		}
	}

	TEST_END();
	return TEST_EXIT_CODE();
}