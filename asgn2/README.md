# Assignment 2 Resources

This directory contains resources for assignment 2:

* test_scripts/ A directory containing test scripts for evaluating your
  assignment.

* starter_code/ A directory containing a library of helper functions

* load_repo.sh a script that can be used to load your asgn2 for testing

* test_repo.sh a script that can be used to test your repository

In the instructions that follow, {path_to_asgn2_dir} is the path to
your asgn2 directory.

Finally, our tests require that you have another package in your
system, named net-tools.  To install it, run:

```
sudo apt install net-tools
```

## Using the helper functions:

Copy the helper function files into your assignment 2 directory:

```
cp starter_code/x86/asgn2_starter_funcs* {path_to_asgn2_dir}

```


You can use the header file, asgn2_starter_funcs.h, in the same way
that you would use any other header.  You can use the static library,
asgn2_starter_funcs.a, like you would an object file.  That is, when
you build your final executable you will include it in the command
line.  For example, to to build my program, `httpserver`, using my
`httpserver.c` file and the `asgn2_starter_funcs.a` library.  I would
execute:

```
clang -o httpserver httpserver.c asgn2_starter_funcs.a
```


## Using the tests

1. Copy the tests to your repository using the supplied load_repo.sh
   script:

```
./load_repo.sh {path_to_asgn2_dir}
```


2. Go to your asgn2 directory:

```
    cd {path_to_asgn2_dir}
```

3. Make your memory binary:

```
    make
````

4. execute test_repo.sh:

```
    ./test_repo.sh
```

This command will print out each test and whether it passed or failed.
If the test passed, you will see a message saying "SUCCESS"; if it
fails you will see a message saying "FAILURE".  You can execute each
test individually as well.  For example, to execute the test
test_scripts/test_xxx.sh, run:

```
./test_scripts/test_xxx.sh
```
