# About this Project

When finished, this Project should be come a Code editor that focuses on Speed and Performance. The UI is kept minimal as possible. Clickable buttons are provided only for the most basic functions. The main input method is by keyboard. The code editor has all necessary tools included to be productive but not nothing more. *"As lean as possible, as bloated as necessary"*. 
The project is written in C++ and uses Direct2D for rendering. Text shaping is done via DirectWrite.

# Structure

```
(root)
  |
  +--assets  -> contains icons, logos and other image files
  |
  +--langs   -> config files for programming languages that the editor supports out of the box (not added to the repository yet)
  |
  +--deps    -> third party dependencies
  |
  +--out     -> default build out (gets created by .\build.ps1)
  |
  +--src     -> source files
  |
  +--test    -> test code
```

# Building

The project uses CMake as primary build system. There is a `build.ps1` in the root folder which is a wrapper around the CMake invocation and few more functions. It has the following parameters:

- `-clean` deleting the output directory
- `-reconfiguire` reconfigure CMake
- `-profile` select the build profile. "debug", "stable", "release". Defaults to debug.
- `-run` run after successful build
- `-test` execute tests after successful build
- `-install` run CMake install. `-installDir` sets the install dirctory.

# Tests

The `.\test` directory contains the all the test code. Tests use a very simple, custom testing framework. Most tests fall into the *end-to-end test* category (though it is also possible to write more isolated *unit tests*). The test framework spawns real instance of the editor. The `PushEvent`-function simulates user input and simulates a full frame of the editor so that the events get processed.

One test-file is what is considered a "Test Suite" in other frameworks. Every test-file contain a number of test functions that perform a series of checks. Test functions need to be non-static and accept either no arguments or a `va_list`. The name of every test functions should be in this pattern: `Test_{SuitName}_{WhatGetsTests}` and the file name should be `tests-{Component}.cc`.
Every test function also needs to be added to the list of test functions in `.\test\tests.h` together with the parameters that should be inoked with. If a test function should be run multiple times with differnt parameters it needs to be added multiple times.
To perform checks the `CHECK_{Op}`- and `REQUIRE_{Op}` macros from `checks.hh` should be used.

An Example:

```c++
void Test_Foo_MyFirstTest(va_list args) {
  const int intParam = va_arg(args, int);
  CHECK_EQ(intParam, 123);
}
```

# Coding style

## Visibility

- all "classes" are actually c++ structs, everything is public

- never write private or protected functions.
  If helper functions are needed, use a free standing static function that takes a pointer to the struct as the first parameter. That parameter should be named *self*:
  
  ```c++
  static void SomeHelperFunction(Foo* self) {
      // do something
  }
  
  void Foo::SomeFunction() {
      SomeHelperFunction(this);
  }
  ```

## Naming

- source file should use *lower-kebab-case*. Header files get the file ending *.hh*, source file *.cc*
- struct/classes use *PascalCase*
- function names use *PascalCase*
- variable names use *camelCase*
- macros use *UPPER_SNAKE_CASE*
- goto labels use *lower_snake_case*

## Indentation & Braces

- indent using tabs
- the opening curly brace goes on the same line as the struct/function/if/for/while/etc:
  
  ```c++
   if (foo) {
       bar();
   } else {
       baz();
   }
  ```

- when initializing a struct the closing brace goes on the same line as the last member with no space between the braket and the member:
  
  ```c++
   auto foo = Bar {
       .x = 0,
       .y = 1};
  ```

- when initializing a struct in one line there should be no space after the opening and before the closing brace:
  
  ```c++
   Bar foo {0, 1};
  ```

- type decorators (pointer * and reference &) belong to the type not the variable
  
  ```c++
    void function(Foo* foo, Bar& bar) { .. }
  ```

## Enums

- enums in the global namespace are discuraged. Put them inside a related struct instead.
- enum names and member names use *PascalCase*, but...
- ...every enum member should have the name of their enum as prefix, seperated by an underscore
- enums should have an "default"-Member with value 0. The exact name of that member may vary ("Unknown" and "None" are often good candidates)
  
  ```c++
  enum Fruits {
       Fruits_Unknown = 0,
       Fruits_Banana,
       Fruits_Apple,
       Fruits_Mango
  };
  ```

## Miscellaneous

- use the typedefs from `basic.hh` wherever possible
- writing template functions/classes are discuraged but may be used if there is no other way
- use of `void*` is explicitly allowed
- use of `goto` is discuraged however not strictly forbidden. Often used to mimic the equivalent of pythons `for..else` construct.
