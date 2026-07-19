clox
====
This is an implementation of the `clox` bytecode interpreter from the [Crafting Interpreters](http://www.craftinginterpreters.com/) book.

## Requirements
- CMake
- A C compiler with C99 support

## Building
This project uses CMake as its build system. Clone the repository and then build it with CMake
```text
git clone https://github.com/AndyPhuHuynh/lox-c
cd lox-c
mkdir build
cd build
cmake ..
cmake --build .
```

## Running
The compiled executable can be run directly. 

If no command line arguments are provided, `clox` starts an interactive REPL.

To execute a Lox program, provide the path to a `.lox` file.

Examples:
```text
# Start the REPL
clox

# Execute a script
clox program.lox
```

On Windows, run `clox.exe` instead.

## Extensions Beyond the Book
This version of the interpreter features a few notable changes compared to the original interpreter featured in the book.

- More than 256 local variables are supported
  - Instructions that reference constants, locals, and other indexed values now have 'LONG' that support larger indices
- The value stack is dynamically allocated instead of fixed-size
- The call frame stack is dynamically allocated instead of fixed-size

### Switch statements
Support for switch statements has been added. 

You can break out of a switch statement early by using the `break` keyword. Cases do not implicitly fall through, so
`break` is only needed when exiting early from within a case.
```lox
switch (i) {
    case 1: {
        print 1;
    }
    case 2: {
        print 2;
    }
    case 3: {
        print 3;
    }
    default: {
        if (i == 4) {
            print 4;
            break;
        }
        print "executing default";
    }
}
```

### Continue and Break statements
You can manage control flow in `for-loops` using the `continue` and `break` keywords. These behave the same as in C.
```lox
for (var i = 0; i < 10; i = i + 1) {
    if (i == 5) continue;
    if (i > 7) break;
}
```

### Object indexing
Support for bracket notation has been added, similar to JavaScript.
```lox
class Object {}
var obj = Object();

// The bottom two lines are equivalent
obj["hello"] = "world"; 
obj.hello = "world";
```