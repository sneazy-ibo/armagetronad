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

#include "doctest.h"
#include "eGameObject.h"
#include "eGrid.h"
#include "eWall.h"

// Mock wall classes for testing - don't need a grid
class MockHarmlessWall : public eWall
{
public:
    MockHarmlessWall() : eWall(nullptr) {}
    bool Splittable() const override { return true; }
    bool Deletable() const override { return true; }
    bool Massive() const override { return false; }
};

class MockDeadlyWall : public eWall
{
public:
    MockDeadlyWall() : eWall(nullptr) {}
    bool Splittable() const override { return true; }
    bool Deletable() const override { return true; }
    bool Massive() const override { return true; }
};

// Test game object that can distinguish between wall types
// We need to use a grid, but we can create a minimal one
class TestGameObject : public eReferencableGameObject
{
public:
    bool alive_;

    TestGameObject(eGrid* grid, const eCoord& p, const eCoord& d, eFace* currentface = NULL)
        : eReferencableGameObject(grid, p, d, currentface, false), // autodelete=false to prevent deletion
          alive_(true)
    {
    }
    
    void Kill() override
    {
        alive_ = false;
        // Don't actually delete ourselves - we want to inspect the state
    }
    
    bool EdgeIsDangerous(const eWall *w, REAL, REAL) const override
    {
        return w && w->Massive();
    }
    
    bool Alive() const override { return alive_; }
};

// Helper to create and manage a minimal grid for testing
class TestGridHelper
{
public:
    eGrid *grid;
    TestGridHelper() {
        grid = new eGrid();
        grid->Create();
    }
    ~TestGridHelper() {
        grid->Clear();
        // Don't delete - destructor is protected
    }
    eFace* GetFace() {
        eCoord startPos(20, 100);
        return grid->FindSurroundingFace(startPos);
    }
};

DOCTEST_TEST_SUITE("eGameObject")
{
    TEST_CASE("Game object interaction with harmless and deadly walls")
    {
        DOCTEST_GIVEN("A test game object with a minimal grid")
        {
            TestGridHelper gridHelper;
            eFace *face = gridHelper.GetFace();
            REQUIRE(face != nullptr);
            
            eCoord startPos(20, 100);
            eCoord direction(1, 0);
            
            TestGameObject obj(gridHelper.grid, startPos, direction, face);
            
            DOCTEST_THEN("The object starts alive")
            {
                CHECK(obj.Alive() == true);
            }
            
            DOCTEST_WHEN("The object passes through a harmless wall")
            {
                MockHarmlessWall harmlessWall;
                obj.PassEdge(&harmlessWall, 1.0, 0.5, 1);
                
                DOCTEST_THEN("The object remains alive")
                {
                    CHECK(obj.Alive() == true);
                }
            }
            
            DOCTEST_WHEN("The object passes through a deadly wall")
            {
                TestGameObject obj2(gridHelper.grid, startPos, direction, face);
                
                MockDeadlyWall deadlyWall;
                obj2.PassEdge(&deadlyWall, 1.0, 0.5, 1);
                
                DOCTEST_THEN("The object is killed")
                {
                    CHECK(obj2.Alive() == false);
                }
            }
        }
        
        DOCTEST_GIVEN("A test game object with EdgeIsDangerous checking")
        {
            TestGridHelper gridHelper;
            eFace *face = gridHelper.GetFace();
            REQUIRE(face != nullptr);
            
            eCoord startPos(20, 100);
            eCoord direction(1, 0);
            
            TestGameObject obj(gridHelper.grid, startPos, direction, face);
            
            DOCTEST_WHEN("Checking if walls are dangerous")
            {
                MockHarmlessWall harmlessWall;
                MockDeadlyWall deadlyWall;
                
                DOCTEST_THEN("Harmless wall is not dangerous")
                {
                    CHECK(obj.EdgeIsDangerous(&harmlessWall, 1.0, 0.5) == false);
                }
                
                DOCTEST_THEN("Deadly wall is dangerous")
                {
                    CHECK(obj.EdgeIsDangerous(&deadlyWall, 1.0, 0.5) == true);
                }
                
                DOCTEST_THEN("NULL wall is not dangerous")
                {
                    CHECK(obj.EdgeIsDangerous(nullptr, 1.0, 0.5) == false);
                }
            }
        }
    }
    
    TEST_CASE("Game object default behavior with walls")
    {
        DOCTEST_GIVEN("A standard eReferencableGameObject (using default EdgeIsDangerous)")
        {
            TestGridHelper gridHelper;
            eFace *face = gridHelper.GetFace();
            REQUIRE(face != nullptr);
            
            eCoord startPos(20, 100);
            eCoord direction(1, 0);
            
            eReferencableGameObject obj(gridHelper.grid, startPos, direction, face, false);
            
            DOCTEST_WHEN("Checking default EdgeIsDangerous behavior")
            {
                MockDeadlyWall deadlyWall;
                
                DOCTEST_THEN("Default EdgeIsDangerous returns true for non-null walls")
                {
                    CHECK(obj.EdgeIsDangerous(&deadlyWall, 1.0, 0.5) == true);
                }
                
                DOCTEST_THEN("Default EdgeIsDangerous returns false for null walls")
                {
                    CHECK(obj.EdgeIsDangerous(nullptr, 1.0, 0.5) == false);
                }
            }
        }
    }
}
