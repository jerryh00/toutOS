#include "StringTest.h"
#include "../include/string.h"

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( StringTest );


void
StringTest::setUp()
{
}


void
StringTest::tearDown()
{
}


void
StringTest::testStrchrNullPtr()
{
    // Setup
    char *s = NULL;
    int c = 'o';
    char *p;

    // Exercise
    p = strchr(s, c);

    // Verify
    CPPUNIT_ASSERT(  p == NULL );
}

void
StringTest::testStrchrFoundChar()
{
    // Setup
    char s[] = "Hello, world!";
    int c = 'o';
    char *p;

    // Exercise
    p = strchr(s, c);

    // Verify
    CPPUNIT_ASSERT(  p == (s + 4) );
}

void
StringTest::testStrchrNotFoundChar()
{
    // Setup
    char s[] = "Hello, world!";
    int c = 'z';
    char *p;

    // Exercise
    p = strchr(s, c);

    // Verify
    CPPUNIT_ASSERT(  p == NULL );
}

void
StringTest::testStrcmpEqual()
{
    int ret;

    // Setup
    char s[] = "Hello, world";

    // Exercise
    ret = strcmp(s, s);

    // Verify
    CPPUNIT_ASSERT(  ret == 0 );
}

void
StringTest::testStrcmpLess()
{
    int ret;

    // Setup
    char s1[] = "Hello, world";
    char s2[] = "Hellp, world";

    // Exercise
    ret = strcmp(s1, s2);

    // Verify
    CPPUNIT_ASSERT(  ret < 0 );
}

void
StringTest::testStrcmpGreater()
{
    int ret;

    // Setup
    char s1[] = "Hello, world";
    char s2[] = "Hello, vorld";

    // Exercise
    ret = strcmp(s1, s2);

    // Verify
    CPPUNIT_ASSERT(  ret > 0 );
}

void
StringTest::testStrcmpShort()
{
    int ret1;
    int ret2;

    // Setup
    char s1[] = "Hello, world";
    char s2[] = "Hello";

    // Exercise
    ret1 = strcmp(s1, s2);
    ret2 = strcmp(s2, s1);

    // Verify
    CPPUNIT_ASSERT(  ret1 > 0 );
    CPPUNIT_ASSERT(  ret2 < 0 );
}

void
StringTest::testStrlen()
{
    int ret1;
    int ret2;

    // Setup
    char s1[] = "";
    char s2[] = "Hello";

    // Exercise
    ret1 = strlen(s1);
    ret2 = strlen(s2);

    // Verify
    CPPUNIT_ASSERT(  ret1 == 0 );
    CPPUNIT_ASSERT(  ret2 == 5 );
}

void
StringTest::testStrcpy()
{
    char *ret;

    // Setup
    char s1[20];
    char s2[] = "Hello, world!";

    // Exercise
    ret = strcpy(s1, s2);

    // Verify
    CPPUNIT_ASSERT(  ret == s1 );
    CPPUNIT_ASSERT(  strcmp(s1, s2) == 0 );
}

void
StringTest::testStrncmpEqual()
{
    int ret;

    // Setup
    char s[] = "Hello, world";

    // Exercise
    ret = strncmp(s, s, 5);

    // Verify
    CPPUNIT_ASSERT(  ret == 0 );
}

void
StringTest::testStrncmpLess()
{
    int ret;

    // Setup
    char s1[] = "Hello, world";
    char s2[] = "Hellp, world";

    // Exercise
    ret = strncmp(s1, s2, 5);

    // Verify
    CPPUNIT_ASSERT(  ret < 0 );
}

void
StringTest::testStrncmpGreater()
{
    int ret;

    // Setup
    char s1[] = "Hello, world";
    char s2[] = "Hello, vorld";

    // Exercise
    ret = strncmp(s1, s2, 8);

    // Verify
    CPPUNIT_ASSERT(  ret > 0 );
}

void
StringTest::testStrncmpShort()
{
    int ret1;
    int ret2;

    // Setup
    char s1[] = "Hello, world";
    char s2[] = "Hello";

    // Exercise
    ret1 = strncmp(s1, s2, 6);
    ret2 = strncmp(s2, s1, 6);

    // Verify
    CPPUNIT_ASSERT(  ret1 > 0 );
    CPPUNIT_ASSERT(  ret2 < 0 );
}

void
StringTest::testStrncpy()
{
    char *ret;
    char *ret3;

    // Setup
    char s1[20];
    char s2[] = "Hello, world!";
    char s3[20];

    // Exercise
    ret = strncpy(s1, s2, 20);
    ret3 = strncpy(s3, s2, 5);

    // Verify
    CPPUNIT_ASSERT(  ret == s1 );
    CPPUNIT_ASSERT(  strncmp(s1, s2, 20) == 0 );
    CPPUNIT_ASSERT(  ret3 == s3 );
    CPPUNIT_ASSERT(  strncmp(s3, s2, 5) == 0 );
}
