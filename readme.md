# lax

[![linux](https://github.com/madeso/lax/actions/workflows/linux.yml/badge.svg)](https://github.com/madeso/lax/actions/workflows/linux.yml) [![windows](https://github.com/madeso/lax/actions/workflows/windows.yml/badge.svg)](https://github.com/madeso/lax/actions/workflows/windows.yml)

Lax is a small scripting language designed to be easily embeded in C++. It is based/inspired on the lox language from [crafting interpreters](http://www.craftinginterpreters.com/). The name means [salmon](https://en.wikipedia.org/wiki/Salmon) in swedish.


## todo
* replace nil with null
* add a vm
* rewrite this page so it's clearer
* split compiler and runtime
* improve "vscode" integration with lsp
* use [kgt](https://github.com/katef/kgt) to output railroad diagrams for syntax documentation inspired by the [awesomeness that is sqlite](https://mobile.twitter.com/captbaritone/status/1553973901251596288?t=FE8L2gjVX_ncbAe_EsPRAA&s=09)
* add iterators and ranges to language
* add switch-like if/else sugar
* add break/continue
* add constants
* introduce string formatting
* replace print statement with a regular function
* add `let` in addtion to `var`
* complete todo comments
* modulus operator
* get more ideas from python and c#
* optional/required typing
* fat arrow functions
* reqork class constructor
```
class Unicorn {
  new Unicorn(name, color) {
    System.print("My name is " + name + " and I am " + color + ".")
  }
}
var fred = new Unicorn("Fred", "palomino")
```

## Current lax features

* use new statement to create classes
* must declare functions and members inside classes
```lax
class Foo
{
  var bar;
  fun foobar()
  {
    return this.bar + 2;
  }
}
```
* added `+=` and similar operators