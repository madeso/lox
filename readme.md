# lox

[![linux](https://github.com/madeso/lox/actions/workflows/linux.yml/badge.svg)](https://github.com/madeso/lox/actions/workflows/linux.yml) [![windows](https://github.com/madeso/lox/actions/workflows/windows.yml/badge.svg)](https://github.com/madeso/lox/actions/workflows/windows.yml)

This repo contains my implementation of the lox language parsers from [crafting interpreters](http://www.craftinginterpreters.com/). Despite the book, both versions will be implemented in C++.


## todo
* complete part two of jlox implementation
* rewrite this page so it's clearer
* split compiler and runtime
* improve "vscode" integration with lsp
* use [kgt](https://github.com/katef/kgt) to output railroad diagrams for syntax documentation inspired by the [awesomeness that is sqlite](https://mobile.twitter.com/captbaritone/status/1553973901251596288?t=FE8L2gjVX_ncbAe_EsPRAA&s=09)

## lox additions/upgrades

* rename language to something else, lax? (swedish/old english/etc for salmon) loxy? loxxy?
* add iterators and ranges
* add switch-like if/else sugar
* add break/continue
* add constants
* add `let` in addtion to `var`
* complete todo comments
* modulus operator
* get more ideas from python and c#
* optional/required typing
* fat arrow functions
* make constructor more like wren
```
class Unicorn {
  construct new(name, color) {
    System.print("My name is " + name + " and I am " + color + ".")
  }
}
var fred = Unicorn.new("Fred", "palomino")

class Unicorn {
  construct brown(name) {
    System.print("My name is " + name + " and I am brown.")
  }
}

var dave = Unicorn.brown("Dave")
```