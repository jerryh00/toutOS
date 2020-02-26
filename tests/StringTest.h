#ifndef STRING_TEST_H
#define STRING_TEST_H

#include <cppunit/extensions/HelperMacros.h>

class StringTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE( StringTest );
  CPPUNIT_TEST( testStrchrNullPtr );
  CPPUNIT_TEST( testStrchrFoundChar );
  CPPUNIT_TEST( testStrchrNotFoundChar );
  CPPUNIT_TEST( testStrcmpEqual );
  CPPUNIT_TEST( testStrcmpLess );
  CPPUNIT_TEST( testStrcmpGreater );
  CPPUNIT_TEST( testStrcmpShort );
  CPPUNIT_TEST( testStrlen );
  CPPUNIT_TEST( testStrcpy );
  CPPUNIT_TEST( testStrncmpEqual );
  CPPUNIT_TEST( testStrncmpLess );
  CPPUNIT_TEST( testStrncmpGreater );
  CPPUNIT_TEST( testStrncmpShort );
  CPPUNIT_TEST( testStrncpy );
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void testStrchrNullPtr();
  void testStrchrFoundChar();
  void testStrchrNotFoundChar();
  void testStrcmpEqual();
  void testStrcmpLess();
  void testStrcmpGreater();
  void testStrcmpShort();
  void testStrlen();
  void testStrcpy();
  void testStrncmpEqual();
  void testStrncmpLess();
  void testStrncmpGreater();
  void testStrncmpShort();
  void testStrncpy();
};

#endif  // STRING_TEST_H
