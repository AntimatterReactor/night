<p align="center">
  <img src="https://github.com/DynamicSquid/night/blob/master/docs/media/night-logo-black.png"/>
</p>

<p align="center">
  <h1>Night</h1>

  <a href="../../actions/actions">
    <img alt="Tests Passing" src="../../workflows/CMake/badge.svg" />
  </a>
  <a href="https://github.com/DynamicSquid/night/issues">
    <img alt="Issues" src="https://img.shields.io/github/issues/DynamicSquid/night?color=0088ff" />
  </a>
  <a href="https://github.com/anuraghazra/github-readme-stats/pulls">
    <img alt="GitHub pull requests" src="https://img.shields.io/github/issues-pr/DynamicSquid/night?color=0088ff" />
  </a>

Night is an interpreted dynamically typed language with a strong control on types.

Night provides compile time definition and type checking, in addition with static types. It can also treat types as first class citizens, allowing the user to have more control over compile time type checking.

</p>

## Current Progress

Fixing website :(

## Getting Started

Night comes in two parts, the interpreter (this repo) and [Dusk](https://github.com/firefish111/dusk) (the package manager).

Currently, Night only supports a source build. Binaries for Windows and Linux are comming soon :)

### Building from Source

Currently, Night is unbuildable on platforms like debian and ubuntu because of the default gcc version they have which is gcc-9

##### 1. Install Dependencies

`g++` (version 10+)<br>
`cmake` (version 3+)

Note: you need to put them on PATH in Windows

##### 2. Clone Repository

If you would like to clone Night and Dusk (recommended), be sure to fetch it's submodule:

```
git clone --recurse-submodules https://github.com/DynamicSquid/night.git
cd night
```

Or if you want to clone Night but not Dusk:

```
git clone https://github.com/DynamicSquid/night.git
cd night
```

##### 3. Compile Night

Windows:

```
cmake -G "MinGW Makefiles" .
cmake --build .
```

Linux:

```
cmake -G "Unix Makefiles" .
cmake --build .
```

##### 4. Run Night

First, create a file called `source.night` where you'll write your code.

Windows:

Windows (cmd):

```
night source.night
```

Linux (bash) and Powershell:

```
./night source.night
```

And you're done!

## Contributing

Contributions are always welcome! Reporting bugs you find, refactoring spaghetti, and writing tutorials on the website are just a few of the ways you can contribute.
