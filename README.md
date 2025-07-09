# MLOX

**MLOX** is a C-based scripting language inspired by [LOX](https://craftinginterpreters.com/), but significantly augmented with additional language features and runtime capabilities. It is designed to be expressive, minimal, and extensible—ideal for learning language design or experimenting with modern scripting constructs.

---

## Overview

While LOX serves as a clean, minimal baseline interpreter, MLOX expands its capabilities in several directions:

- **Ternary (`? :`) and extended operators**: including bitwise, shift, and modulo.
- **Full object-oriented model**: even primitive types are first-class objects with methods.
- **Modern control structures**: such as `while`, and `for-in`/`foreach` loops.
- **Native string manipulation**: slicing, searching, and concatenation.
- **Math library**: including trigonometric functions.
- **File I/O and user input**: read from files or prompt interactively.
- **Networking support**: sockets and basic TCP/UDP abstractions (planned).

---

## Example

```mlox
fn greet(name) {
  print "Hello, " + name + "!";
}

for (person in ["Alice", "Bob", "Charlie"]) {
  greet(person);
}

var angle = 3.14159 / 2;
print "sin(π/2) = " + sin(angle);
