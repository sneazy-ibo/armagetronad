/*
Armagetron Advanced -- a Tron clone in 3D
Copyright (C) 2000  Manuel Moos (manuel@moosnet.de)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

// This file demonstrates the *ideal* coding style for tests.

// Comments starting with `FYI` in them are meant as comments just to describe
// what is going on HERE, they are not meant as templates to include in actual code.

#include "doctest.h"

#include "CodingStyle.h"

// FYI tests use BDD patterns whenever appropriate
TEST_SUITE("CodingStyle")
{
    TEST_CASE("Counting works")
    {
        GIVEN("A counter")
        {
            // FYI local varaibles are camelCase.
            cCounter counter; // the counter

            WHEN("Left at default")
            {
                THEN("It starts at zero")
                {
                    CHECK(0 == counter.GetCount());
                }
                AND_THEN("It cannot count down")
                {
                    CHECK(!counter.TryCountDown());
                    CHECK(0 == counter.GetCount());
                }
            }

            WHEN("Counting up")
            {
                counter.CountUp();

                THEN("It goes to one")
                {
                    CHECK(1 == counter.GetCount());
                }
                AND_THEN("It can count down")
                {
                    CHECK(counter.TryCountDown());
                    CHECK(0 == counter.GetCount());
                }
            }
        }
    }

    TEST_CASE("Shallow Copy")
    {
        GIVEN("A filled shallow copy holder")
        {
            {
                CHECK(0 == cReferenceCounted::GetNumberOfObjects());

                cShallowCopy holder{new cReferenceCounted};

                CHECK(1 == cReferenceCounted::GetNumberOfObjects());

                WHEN("Doing nothing")
                {
                    THEN("We have one object")
                    {
                        CHECK(1 == cReferenceCounted::GetNumberOfObjects());
                    }
                }

                WHEN("Making a copy")
                {
                    cShallowCopy copy{holder};

                    THEN("We still have one object")
                    {
                        CHECK(1 == cReferenceCounted::GetNumberOfObjects());
                    }
                    AND_WHEN("Resetting the original")
                    {
                        holder.SetTarget(nullptr);

                        THEN("We still have one object")
                        {
                            CHECK(1 == cReferenceCounted::GetNumberOfObjects());
                        }
                        AND_WHEN("We also reset the copy")
                        {
                            copy.SetTarget(nullptr);

                            THEN("We have no objects left")
                            {
                                CHECK(0 == cReferenceCounted::GetNumberOfObjects());
                            }
                        }
                    }
                }
            }
            THEN("In the end, no object remains")
            {
                CHECK(0 == cReferenceCounted::GetNumberOfObjects());
            }
        }
    }

    TEST_CASE("Deep Copy")
    {
        GIVEN("A filled deep copy holder")
        {
            {
                CHECK(0 == cReferenceCounted::GetNumberOfObjects());

                cDeepCopy holder{new cReferenceCounted};

                CHECK(1 == cReferenceCounted::GetNumberOfObjects());

                WHEN("Doing nothing")
                {
                    THEN("We have one object")
                    {
                        CHECK(1 == cReferenceCounted::GetNumberOfObjects());
                    }
                }

                WHEN("Making a copy")
                {
                    cDeepCopy copy{holder};

                    THEN("We have two objects")
                    {
                        CHECK(2 == cReferenceCounted::GetNumberOfObjects());
                    }
                    AND_WHEN("Resetting the original")
                    {
                        holder.SetTarget(nullptr);

                        THEN("We have one object left")
                        {
                            CHECK(1 == cReferenceCounted::GetNumberOfObjects());
                        }
                        AND_WHEN("We also reset the copy")
                        {
                            copy.SetTarget(nullptr);

                            THEN("We have no objects left")
                            {
                                CHECK(0 == cReferenceCounted::GetNumberOfObjects());
                            }
                        }
                    }
                }

                WHEN("Moving the holder")
                {
                    cDeepCopy copy{std::move(holder)};

                    THEN("We still have one object")
                    {
                        CHECK(1 == cReferenceCounted::GetNumberOfObjects());
                    }
                    AND_WHEN("Resetting the original")
                    {
                        holder.SetTarget(nullptr);

                        // FYI it is an implementation detail what would happen if we reset the copy instead.

                        THEN("We still have the same object")
                        {
                            CHECK(1 == cReferenceCounted::GetNumberOfObjects());
                        }
                        AND_WHEN("We also reset the copy")
                        {
                            copy.SetTarget(nullptr);

                            THEN("We have no objects left")
                            {
                                CHECK(0 == cReferenceCounted::GetNumberOfObjects());
                            }
                        }
                    }
                }
            }
            THEN("In the end, no object remains")
            {
                CHECK(0 == cReferenceCounted::GetNumberOfObjects());
            }
        }
    }
}
