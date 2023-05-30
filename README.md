# Stream Buffer

Read data from the standard input stream (`stdin`) and write it back to the standard output stream (`stdout`).

The implementation uses separate threads for reading/writing and an internal ring buffer.

## Description

Writing text to the terminal, e.g. by using the [`printf()`](https://cplusplus.com/reference/cstdio/printf/) family of functions, can be surprisingly slow &ndash; especially on the Windows platform. This means that text output can easily become the performance bottleneck of console application! This is mostly evident for console application that need to output large amounts of text.

As it turns out, passing the text output through and additional ring buffer can **greatly** improve the performance 😏

## Usage

Simply pass the output of the source program to the `streambuff` utility via [*pipe*](https://en.wikipedia.org/wiki/Pipeline_(Unix)) operator:

```
some_program.exe [parameters] | streambuff.exe
```

## License

Copyright (c) 2023 “dEajL3kA” &lt;Cumpoing79@web.de&gt;  
This work has been released under the MIT license. See [LICENSE.txt](LICENSE.txt) for details!

### Acknowledgement

Using [Buffer icons](https://www.flaticon.com/free-icons/buffer) created by Muhammad_Usman &ndash; Flaticon.
