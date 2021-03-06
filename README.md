# CodeParser

CodeParser is a package for parsing Wolfram Language source code as abstract syntax trees (ASTs) or concrete syntax trees (CSTs).
CodeParser is useful for inspecting code, formatting code, and instrumenting code (for e.g., coverage reporting or profiling), and much more!

CodeParser has many key features:
* Understands practically entire Wolfram Language syntax.
* Fast native library implementation.
* Tested with combination of suite of hand-written tests and fuzz testing.
* Gracious error handling and recovery


```
Needs["CodeParser`"]

CodeParse["1+1"]
```
```
Out[2]= ContainerNode[String, {CallNode[LeafNode[Symbol, "Plus", <||>], {LeafNode[Integer, "1", <|Source -> {{1, 1}, {1, 2}}|>], LeafNode[Integer, "1", <|Source -> {{1, 3}, {1, 4}}|>]}, <|Source -> {{1, 1}, {1, 4}}|>]}, <||>]
```

[Parsing the Wolfram Language from WTC 2019: Watch Video](https://www.wolfram.com/broadcast/video.php?v=2908)

[Parsing the Wolfram Language from WTC 2019: Download Presentation](https://files.wolframcdn.com/pub/www.wolfram.com/technology-conference/2019/Thursday/2019BrentonBostickParsingTheWL.nb)


## Setup

Install CodeParser from the paclet server:
```
PacletInstall["CodeParser"]
```

Make sure that the paclet can be found on your system:
```
Needs["CodeParser`"]
```

## Using CodeParser

After CodeParser is installed, it can be used.

```
Needs["CodeParser`"]

CodeParse["1+1"]
```
```
Out[2]= ContainerNode[String, {CallNode[LeafNode[Symbol, "Plus", <||>], {LeafNode[Integer, "1", <|Source -> {{1, 1}, {1, 2}}|>], LeafNode[Integer, "1", <|Source -> {{1, 3}, {1, 4}}|>]}, <|Source -> {{1, 1}, {1, 4}}|>]}, <||>]
```

### Command-line tool (Optional)

An optional `codeparser` command-line tool is also built and can be used.

```
cmake -DBUILD_EXE=ON ..
cmake --build . --target codeparser-exe

$cpp/src/exe/codeparser
>>> 1+1
InfixNode[Plus, {LeafNode[Integer, "1", <|Source->{{1, 2}, {1, 2}}|>], LeafNode[Integer, 1, <|Source->{{1, 3}, {1, 4}}|>]}, <|Source->{{1, 1}, {1, 4}}|>]

>>>
```
