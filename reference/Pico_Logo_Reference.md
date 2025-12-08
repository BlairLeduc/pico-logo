Pico Logo
=========

After flashing Pico Logo to your Pico, you should see:

```
Copyright 2025 Blair Leduc
Welcome to Pico Logo.
?_
```

The question mark, `?` is the _prompt_. When the prompt is on the display, you can type something. The flashing underscore, `_` is the cursor. It appears when Logo wants you to type something and shows where the next character you type will appear.


# An Introduction {#introduction}

## Logo Programs

A Logo program is a collection of procedures. 

For example, a program to draw a house consists of these procedures: `house`, `box`, `tri`, `right`, `forward`, and `repeat`. Of these, the last three are _primitives_. The first three are user-defined procedures, built out of Logo primitives.

```logo
to house
box 
forward 50
right 30
tri 50 
end 

to box 
repeat 4 [forward 50 right 90] 
end

to tri :size
repeat 3 [forward :size right 120] 
end 
```

We are assuming that you have had some experience with Logo and have built up an intuitive model of what Logo is about. Here we give a more formal model. This model is just another way to think about Logo. It is not meant to replace vour current way of thinking, but rather to enhance it. 

## Formal Logo

The actual Logo system is very complicated. It will help if we consider a formal description of Logo, and then Logo as it really is. We begin our formal description by restricting ourselves to a part of Logo, which we'll call _formal Logo_; we then relax the restrictions in order to describe the language you actually use, which we'll call _relaxed Logo_.

Every instruction in formal Logo works without change in relaxed Logo. Conversely, anything that can be done in Logo can be done in formal Logo, but you will immediately recognize situations where formal Logo forces idioms that no one would actually use. For example, if you are experienced in Logo you will notice that numbers look peculiar in formal Logo: they are quoted. Since numbers are words, you can quote them in relaxed Logo as well. But we do so very rarely; in relaxed Logo, numbers are self-quoting.

**Procedures and inputs:** We assume that you know what procedures are, at least on an intuitive level. Here we develop some more precise ways to talk about them. 

In formal Logo, every procedure requires a definite number of inputs. These inputs are always one of two kinds: they may be _words_ or they may be _lists_. They may be given directly, as in `forward "100`, or indirectly through the mediation of another procedure, as in `forward sum "60 “40`. Both of these examples have the same effect. If an input is given directly as a word, it must be preceded by a quote mark, as in `print "forward`. If it is given directly as a list, it is surrounded by square brackets, as in `print [How are you?]`. 

**Words and lists:** A word is made up of characters. A list is made up of elements enclosed in square brackets, with spaces between elements; an element is either a word (without quotes) or a list. 

**Naming things:** The name of a procedure, input, or variable is a word. Logo does not care if a name is in lower or upper case. For example, the following are regarded as being the same:

```logo
forward 100
FORWARD 100
```

The name is remembered using the case that is used in the definition.

**Property Lists:** Any Logo word can have a _property list_ associated with it. A property list consists of an even number of elements. Each pair of elements consists of a property, and its value, a word or a list.

**Expressions:** A procedure name followed by the required number of inputs is an _expression_. More generally, an _expression_ is 

- a quoted word, 
- list, or 
- an (unquoted) procedure name followed by as many expressions as the procedure requires. 

If this seems complicated, look at an example. `repeat` is a procedure that requires two inputs. 

This is an expression:

```logo
repeat "3 [fd 10] 
```

The following is also an expression:

```logo
repeat sum "2 "1 [fd "10]
```

In this case the first input is not a quoted word, but an expression involving another procedure, `sum`. 

The following is also an expression:

```logo
repeat sum "3 "1 sentence "fd "10 
```

**Commands and operations:** There are two kinds of procedures in Logo. Those (like `sum`) that produce a Logo object are called _operations_. The others (like `print`) are called _commands_. 

With these definitions we can define a Logo instruction.

A Logo _instruction_ is a particular kind of expression. It starts with a procedure name, and that procedure must be a command. All other procedures in the expression must be operations. 

Why does it matter whether an expression is an instruction or not? Consider

```logo
sum "3 "4 
```

This is an expression, but it is not a Logo instruction.

`sum` will produce a number; but simply writing `sum "3 "4` does not state what is to be done with the number. In fact, Logo will give an error message saying `I don't know what to do with 7`. Some programming languages would allow this and simply print the `7`. In designing Logo we preferred to make every important act explicit; if you want something to be printed you should say so. 

## Relaxed Logo

Everything that can be done in Logo can be done using formal Logo. But we have allowed some other idioms, either to make the language feel more natural or to eliminate large amounts of typing.

Here we list some important relaxations of Logo. Others are mentioned in the body of the manual. See also [Parsing](#parsing). 

**Numbers:** In formal Logo, all words used as direct inputs, including numbers, must be quoted. In relaxed Logo, numbers are self-quoting: the quote marks are unnecessary. For example, 

```logo
sum 3 4 
```

A _number_ is a word made up of digits; it may also contain a minus sign, a period, and an `E` or `N`. See (Arithmetic Operations)[#arithmetic-operations].

```logo
?print 1e4
10000
?print 1n4
0.0001
```

**Infix procedures:** Formal Logo uses only prefix procedures. Thus addition is expressed by `sum "3 “4`. In relaxed Logo you can also use the infix form `3 + 4`. Similarly with multiplication, subtraction, and division. See [Arithmetic Operations](#arithmetic-operations). 

**Variable number of inputs:** In formal Logo, every procedure has a fixed number of inputs. In relaxed Logo, several procedures can have variable numbers of inputs. When you use other than the 
expected number of inputs you must enclose the expression in parentheses: 

```logo
(sum 3 4 5 6 7 8) 
```

**Dots:** A familiar feature of relaxed Logo is the use of dots (`:`). In formal Logo, `:x` is written as `thing "x`. 

**Parentheses for grouping:** Logo has parentheses so that you can explicitly tell Logo how to Group 
things. For example, consider 

```logo
25 + 20 / 5
```

Should the addition be done first, producing `9`, or should the division be done first, producing `29`? Relaxed Logo follows the traditional mathematical hierarchy, in which multiplication and division are done before addition and subtraction; thus this operation first divides `20` by `5`, and produces `29`. To group the numbers so that addition is done first, parentheses must be used: 

```logo
(25 + 20) / 5 
```

**Commands and operations:** In formal Logo, a given procedure is either a command or an operation, not both. In relaxed Logo, a procedure can be sometimes a command and sometimes an operation. `run` and `if` are examples of this. See [Flow of Control](#flow-of-control). 

**The command `to`:** In formal Logo, you define a procedure with the command `define`; it takes two inputs, a word (the procedure name) and a list (the definition). All the usual rules about quotes and brackets apply without exception:

```logo
define "square [[ ] [repeat 4 [fd 100 rt 90]]] 
```

In relaxed Logo, you can define a procedure with `edit` and `to`. The command `to` is unusual in two ways. It automatically quotes its inputs, and it allows you to type in the lines of the procedure interactively: 

```logo
?to square :size
>repeat 4 [fd :size rt 90] 
>end
``` 

As shown, when interactively defining a procedure using the command `to`, the prompt changes to `>` until a line starting with `end` is encountered ending the definition of the procedure. This provides feedback that you are in the middle of defining a procedure and is the only time when this prompt is used.

## A Further Note on Operations 

In talking about formal Logo we have been using the word _produce_: a procedure _produces_ an object (called the _output_ of the procedure). In traditional Logo terminology we say that the procedure _outputs_ an object. 


## Logo Objects 

There are two types of Logo _things_ (or _objects_): _words_ and _lists_. 

A word, such as `television` or `617`, is made up of characters (letters, digits, punctuation). A number is a kind of word.

A list, such as `[rabbits television 7 [ears feet]]`, is made up of Logo objects, each of which can be either a word or a list. See [Words and Lists](#words-and-lists). 

## Variables: Some General Information

A _variable_ is a container that holds a Logo object. The container has a name and a value. The object held in the container is called the variable's _value_. You create a variable in one of two ways: either by using the `make` or `name` command, or by using procedure inputs.

Logo has two kinds of variables: local variables and global variables. Variables used as procedure inputs are local to that procedure. They exist only as long as the procedure is running, and will disappear from your workspace after the procedure stops running.

Normally a variable created by `make` is a global variable. The `local` command lets you change those variables into local variables. This can be very useful if you want to avoid cluttering up your workspace with unwanted variables.

```logo
to yesno: question
local "answer
pr :question
make "answer first readlist
(if equal? :answer "yes [output "true])
output "false
end
```

## How You Might Think about Quotes 

The role of quotes is best understood through the following example:

```logo
?print "heading
heading
?print heading 
180
``` 

In the first case the quotation mark (`"`) indicates that the word `heading` itself is the input to `print`.

In the second case the input is not quoted. It is therefore interpreted as a procedure, which provides the input to `print`. 

## How You Might Think about MAKE

The effect of the command

```logo
make "bird "pigeon
```

can be thought of as follows. A container is given a name: `bird`. The word `pigeon` is put into the container.

Once this is done the operation `thing` has the following effect:

```logo
thing "bird
```

produces `pigeon`.

Dots are similar to `thing`; for example, 

```logo
:bird
```
produces `pigeon`. 

We can talk about the same situation in several ways: 

`bird` is a _variable_; `pigeon` is its _value_. 

`bird` is a _name_ of `pigeon`. 

`bird` is _bound_ to `pigeon`. 

`pigeon` is the _thing_ of `bird`. 

You can change what it is in the container; if you type

```logo
make "bird "sparrow 
```

then the container `bird` contains the word `sparrow` instead of the word `pigeon`. 

## How to Think about Procedures You Define and their Inputs

Procedures you define can have inputs. When you define a procedure, its title line specifies how many inputs it has and a name for each one. The inputs are put into the containers designated on the title line of the procedure definition in the order in which they occur. For example, the following procedure makes the turtle draw various polygons, depending upon what inputs are used. 

```logo
to poly :step :angle
forward :step 
right :angle 
poly :step :angle 
end 
```

Whatever inputs you choose—for example, `50` and `90`—are put into the specified containers. The first input is always put in the container `step`; the second input is always put in the container `angle`. The first input variable, `step`, controls the size of the figure. The second input variable, `angle`, controls the shape of the figure. `poly 50 90` makes the turtle draw a square; `poly 50 144` makes the turtle draw a pentagram; `poly 50 120` makes the turtle draw a triangle. 

If the input containers already have objects in them when the procedure is run, the objects are removed but are saved. They are restored when the procedure is done. In other words, the procedure _borrows_ the container and leaves it in its original condition when done with it. This is what is meant by saying that a variable is _local_ to the procedure within which it occurs. 

For example: 

```logo
?make "sound "crash 
?print :sound 
crash 
```

Now we run a procedure called `animal`; its input variable is `sound`. 

```logo
to animal :sound 
if :sound = "meow [pr "Cat stop] 
if :sound = "woof [pr "Dog stop] 
pr [I don't know] 
end
```

If we type `animal "woof`, then `woof` is put into the `sound` container for the duration of the `animal` procedure. Afterwards, `crash` is restored:

```logo
?make "sound "crash 
?print :sound 
crash
?animal "woof 
Dog 
?print :sound 
crash 
```

## Another Way to Talk about Procedures 

When you use a procedure, we also say that you _call_ that procedure. Procedures you define call other procedures. For example, the `poly` procedure above calls other procedures (`forward` and `right`). 

`poly` also calls itself. This is called a _recursive_ call. 

## Review of Special Characters 

_Quotation marks_, or _quotes_, `”`, used immediately before a word, indicate that it is being used _as a word_, not as the name of a procedure or the value of a variable. 

A _colon_, known as _dots_, `:`, used immediately before a word, indicates that the word is to be taken as the name of a variable and produces the value of that variable. 

_Brackets_, `[` `]`, are used to surround a list.

_Parentheses_, `(` `)`, are necessary to group things in ways that Logo ordinarily would not, and to vary the number of inputs for certain procedures. Both of these uses are described above in this introduction. 

A _backslash_, `\`, tells Logo to interpret the character that follows it literally _as a character_, rather than keeping some special meaning it might have. For instance, suppose you wanted to use `3[a]b` as a single _word_. You need to type `3\[a\]b` in order to avoid Logo’s usual interpretation of the brackets as the envelope around a list. You have to backslash `[`,`(`,`]`,`)`,`+`,`-`,`*`,`/` and `\` itself. 

# Parsing 

When you type a line at Logo, it recognizes the characters as words and lists, and builds a list with is Logo’s internal representation of the line. This process is called _parsing_. The list is similar to the list that would be output by `readlist`. This section will help you understand how lines are parsed. 

## Words

A word is made up of characters. Here are some examples of words:

- Hello
- x
- 314
- 3.14
- 1e4
- R2D2
- Piglatin
- Pig.latin
- Pig-latin (typed as `Pig\-Latin`)
- Hi there (typed as `Hi\ there`)
- Who?
- !NOW!
- St*rs (typed as `St\*rs`)

Each character is an element of the word. The word `Hen3ry` contains six elements:

```
H   e   n   3   r   y
```

Words can contain any character and this includes alphanumeric characters, spaces, delimiters, tabs, and punctuation. Delimiters must be escaped by a backslash "`\`" character.

## Delimiters and Spacing

A word is usually _delimited_ only by spaces or tabs: That is, there is a space or tab before the word and a space or tab after the word; they set the word off from the rest of line. There are a few other delimiting characters:

```
[ ] ( ) + - * / = < > 
```

You need not type a space between a word and any of these characters. For example, to find out how this line is parsed: 

```logo
if 1<2[print(3+4)/5][print :x+6] 
```
type

```logo
?to testit 
>if 1<2[print(3+4)/5][print :x+6]
>end 
?po "testit
to testit
if 1 < 2 [print (3 + 4) / 5] [print :x + 6]
end
```

And if you define a procedure to contain a line in the first form, you will see that Logo has converted it into the second. 

To treat any of the characters mentioned above as a normal alphabetic character, put a backslash "`\`" before it. For example: 

```logo
?print "Good\-bye
Good-bye
?print "San\ Francisco
San Francisco
```

Note that the quotation mark character (") and the colon (:not word delimiters.

You can also have an empty word, which is a word with elements. You type in the empty word by typing `"`.

## Infix Procedures 

The following characters are the names of infix procedures. You write the name between the two inputs, but Logo considers the procedures to have two inputs. 

```
+ - * / = < > 
```

## Brackets and Parentheses

Left bracket ”`[`” and right bracket ”`]`” indicate the start and end of a list or sublist.

```logo
[Hello there, old chap]
[x y z]
[Hello]
[[House Maison] [Window Fenetre] [Dog Chien]]
[HAL [QRZ] [Belinda Moore]]
[1 [1 2] [17 2]]
```

The list `[Hello there, old chap]` contains four elements:

1. Hello
2. there
3. old
4. chap

Note that the list `[1 [1 2] [17 2]]` contains only three elements, not six. The second and third elements also are lists:

1. 1
2. [1 2]
3. [17 2]

The list `[]`, a list with no elements, is an _empty list_. There also exists an _empty word_, which is a word with no elements. You type in the empty word by typing a quotation mark followed by a space or a right bracket "`]`". See the `equal?` operation in for examples of both the empty list and the empty word.

Parentheses group things in ways Logo ordinarily would not, and vary the number of inputs for certain primitives. 

If the end of a Logo line is reached (that is, the `ENTER` key is pressed) and brackets or parentheses are still open, Logo closes all sublists or expressions. For example: 

```logo
?repeat 4 [print [This [is [a [test 
This [is [a [test]]] 
This [is [a [test]]] 
This [is [a [test]]] 
This [is [a [test]]] 
```

If Logo finds a right bracket or parenthesis for which there was no corresponding left bracket or parenthesis, Logo returns an error: 

```logo
?]print "ABC
Unexpected ']'
?print 2 + 3)
Unexpected ')'
```

## Quotation Marks and Delimiters 

Normally, you have to put a backslash (\) before the characters `[`,`]`,`(`,`)`, and `\` itself. But the first character after a quotation mark (") does not need to have a backslash preceding it. For example: 

```logo
?print "*
*
```

If a delimiter occupies any position but the first one after the quotation mark, it must have a backslash preceding it. For example: 

```logo
?print "****
Not enough inputs to *
```

The only exception to the above general rule is brackets (`[` `]`). If you want to put a quotation mark before a bracket, you must always include a backslash between the quotation mark and the bracket. For example: 

```logo
?print "[ 
You don't say what to do with [] 
?print "\[
[ 
```

```logo
?repeat 4 [print "]




```

Four empty lines are produced since the `"]` is recognised as an emoty word followed by the closing bracket.


## The Minus Sign 

The way in which the minus sign "`-`” is parsed is also a little strange. The problem here is that one character is used to represent two different things:

1. as part of a number to indicate that it is negative, as in `-3` 
2. as a procedure of one input, called _unary minus_, which outputs the additive inverse of its input, as in `-XCOR` or `-:DISTANCE` 
3. as a procedure of two inputs, which outputs the difference between its first input and its second,as in `7-3` and `XCOR-YCOR` 

The parser tries to be clever about this potential ambiguity and figure out which one was meant by the following rules:

1. If the ”`-`” immediately precedes a number, and follows any delimiter except right parenthesis ”`)`”, the number is parsed as a negative number. This allows the following behavior:
  - `print sum 20-20` (parses as 20 minus 20)
  - `print 3*-4` (parses as 3 times negative 4) 
  - `print (3+4)-5` (parses as 3 plus 4 minus 5) 
  - `first [-3 4]` (outputs —3) 
2. If the ”`-`” immediately precedes a word or left parenthesis ”`(`”, and follows any delimiter except right parenthesis, it is parsed as the unary minus procedure:
  - `setpos list :x -:y`
  - `setpos list ycor -xcor`
3. In all other cases, ”`-`” is parsed like the other infix characters—as a procedure with two inputs: 
  - `print 3-4` (parses as 3 minus 4)
  - `print 3 - 4` (parses exactly like the previous example) 
  - `print - 3 4` (procedurally the same as the previous example) 

# Difference from other Logo interpreters

Comments are not supported in Pico Logo. A cooment procedure can be defined to support comments:

```logo
to ; :comment
end
```

This can be used:

```logo
?; [This is a comment]
?to increment :var
>make :var (thing :var)+1 ; [Increement by one]
>end
```

Line continuation characters are not supported.

Words with internal spaces are created using the "`\`" character, not using the veritcal bar notation.

`ifelse` is not a primitive. Use `(if predicate list1 list2)` instead:

```logo
to ifelse :predicate :list1 :list2
(if :predicate :list1 :list2)
end
```

# Words and Lists {#words-lists}

## butfirst (bf)

butfirst _object_
bf _object_

`operation`

`butfirst` outputs all but the first element of _object_. `butfirst` of the empty word or the empty list is an error.


## butlast (bl)

butlast _object_
bl _object_

`operation`

`butlast` outputs all but the last element of _object_.


## first

first _object_

`operation`

`first` outputs the first element of _object_. `first` of the empty word or the empty list is an error. Note that `first` of a word is a single character; `first` of a list can be a word or a list.


## item

item _integer_ _object_

`operation`

`item` outputs the element of _object_ whose position within _object_ corresponds to _integer_. For example, if _integer_ is 3, `item` outputs the third element in the object. _Object_ is a word or a list. An error occurs if _integer_ is greater than the length of _object_ or if _object_ is the empty word or list.


## last

last _object_

`operation`

`last` outputs the last element of _object_. `last` of the empty word or the empty list is an error.


## member

member _object1_ _object2_

`operation`

`member` outputs the part of _object2_ in which _object1_ is the first element. If _object1_ is not an element of _object2_, `member` outputs the empty list or the empty word. This operation is useful for accessing information i n a file or for sorting long lists.


## fput

fput _object_ _list_

`operation`

The `fput` (for first put) operation outputs a new list formed by putting _object_ at the beginning of _list_.


## list

list _object1_ _object2_
(LIST _object1_ _object2_ _object3_ _object4_ ...)

`operation`

The `list` operation outputs a list whose elements are _object1_,
_object2_, and so on.


## lput

lput _object_ _list_

`operation`

The `lput` (for last put) operation outputs a new list formed by
putting _object_ at the end of _list_.


## parse

parse _word_

`operation`

`parse` outputs a list that is obtained from parsing _word_. `parse` is useful for converting the output of `readword` into a list.


## sentence (se)

sentence _object1_ _object2_
(sentence _object1_ _object2_ _object3_ ...)
se _object1_ _object2_
(se _object1_ _object2_ _object3_ ...)

`operation`

`sentence` outputs a list made up of the contents in its inputs.


## word

word _word1_ _word2_
(word _word1_ _word2_ _word3_ ...)

`operation`

`word` outputs a word made up of its inputs.


## ascii

ascii _character_

`operation`

`ascii` outputs the American Standard Code for Information Interchange (ASCII) code for _character_. If the input word contains more than one character, `ascii` uses only its first character. Also see `char`.


## before? (beforep)

before? _word1_ _word2_
beforep _word1_ _word2_

`operation`

`before?` outputs `true` if _word1_ comes before _word2_. To make the comparison, Logo uses the ASCII codes of the characters in the words. Note that all uppercase letters come before all lowercase letters.


## char

char _integer_

`operation`

The `char` operation outputs the character whose ASCII code is _integer_. An error occurs if _integer_ is not the ASCIl code for any character.


## count

count _object_

`operation`

`count` outputs the number of elements in _object_, which is a word or a list.


## empty? (emptyp)

emptyp _object_

`operation`

`emptyp` outputs `true` if _object_ is the empty word or the empty list; otherwise it outputs `false`.


## equal? (equalp)

equal? _object1_ _object2_
equalp _object1_ _object2_

`operation`

`equal?` outputs `true` if _object1_ and _object2_ are equal numbers, identical words, or identical lists; otherwise `equal?` outputs `false`. This operation is equivalent to the equal sign (`=`).


## list? (listp)

list? _object_
listp _object_

`operation`

`list?` outputs `true` if _object_ is a list; otherwise it outputs `false`.


## member? (memberp)

member? _object1_ _object2_
memberp _object1_ _object2_

`operation`

`member?` outputs `true` if _object1_ is an element of _object2_; otherwise it outputs `false`.


## number? (numberp)

number? _object_
numberp _object_

`operation`

`number?` outputs `true` if _object_ is a number; otherwise it outputs `false`.


## word? (wordp)

wordp _object_

`operation`

`word?` outputs `true` if _object_ is a word; otherwise it outputs `false`. A self-quoted number is word.


## lowercase

lowercase _word_

`operation`

`lowercase` outputs _word_ in all lowercase letters.


## uppercase

uppercase _word_

`operation`

`uppercase` outputs _word_ in all uppercase letters.



# Arithmatic Operations {#arithmetic-operations}

## arctan

arctan _number_

`operation`

Outputs the arctangent of _number_ in degrees.


## cos

cos _number_

`operation`

Outputs the cosine of _number_ in degrees.


## difference

difference _number1_ _number2_

`operation`

Outputs _number2_ subtracted from _number1_.


## int

int _number_

`operation`

Returns the integer part of _number_; any decimal part is stripped off. No rounding occurs when `int` is used (contrast this with the `round` operation described later in this chapter).


## intquotient

intquotient _integer1_ _integer2_

`operation`

`intquotient` outputs the result of dividing _integer1_ by _integer2_, truncated to an integer. An error occurs if _integer2_ is zero. If either input is a decimal number, it is truncated.


## product

product _number1_ _number2_
(product _number1_ _number2_ _number3_ ...)

`operation`

Outputs the product of its inputs. It is equivalent to the `*` infix-form operation. With one input, `product` outputs its input.


## quotient

quotient _number1_ _number2_

`operation`

Outputs the result of dividing _number1_ by _number2_. It is equivalent to the `/` infix-form operation. _Number2_ must not be zero. If it is, an error occurs.


## random

random _integer_

`operation`

Outputs a random non-negative integer less than _integer_.


## remainder

remainder _integer1_ _interger2_

`operation`

Outputs the remainder obtained when _integer1_ is divided by _integer2_. The remainder is always an integer. If _integer1_ and _integer2_ are integers, this is _integer1_ mod _integer2_. If _integer1_ and _integer2_ are not integers, they are truncated. _Integer2_ must not be zero. If it is, an error occurs.

## round

round _number_

`operation`

Outputs _number_ rounded off to the nearest integer. The maximum integer is 2,147,483,647.


## sin

sin _number_

`operation`

Outputs the csine of _number_ in degrees.


## sqrt

sqrt _number_

`operation`

Outputs the square root of _number_. The value _number_ must not be negative or an error will occur.


## sum

sum _number1_ _number2_
(sum _number1_ _number2_ _number3_ ...)

`operation`

Outputs the sum of its inputs. `sum` is equivalent t o the `+` infix-form operation. With one input, `sum` outputs its input.



# Conditionals and Control of Flow {#conditionals-control-flow}

## true

true

`operation`

Outputs `"true`. In Logo, boolean truth is represented by the word `true`. 


## false

false

`operation`

Outputs `"false`. In Logo, boolean false is represented by the word `false`.

## if

if _predicate_ _list1_
(if _predicate_ _list1_ _list2_)

`command` or `operation`

If _predicate_ is `true`, Logo runs _list1_. If _predicate_ is `false`,Pico Logo runs _list2_ (if present). In either case, if the selected list outputs something, the `if` is an operation. If the list outputs nothing, the `if` is a command.


## iffalse (iff)

iffalse _list_
iff _list_

`command`

`iffalse` runs _list_ if the result of the most recent `test` was `false`, otherwise it does nothing. Note that if `test` has not been run in the same procedure or a superprocedure, or from top level, `iffalse` does nothing.


## iftrue (ift)

iftrue _list_ 
ift _list_ 

`command`

`iftrue` runs _list_ if the result of the most recent `test` was `true`, otherwise it does nothing. Note that if `test` has not been run in the same procedure or a superprocedure, or from top level, `iftrue` does nothing.


## test

test _predicate_

`command`

`test` remembers whether _predicate_ is `true` or `false` for subsequent use by `iftrue` or `iffalse`. Each `test` is local to the procedure in which it occurs.


## co

co

`command`

The `co` (for continue) command resumes running of a procedure after a `pause` or `ESC`, continuing from wherever the procedure paused.


## output (op)

output _object_
op _object_

`command`

The `output` command is meaningful only when it is within a procedure, not at top level. It makes _object_ the output of your procedure and returns control to the caller. Note that although `output` is itself a command, the procedure containing it is an operation because it has an output. Compare with `stop`.


## pause

pause

`command` or `operation`

The `pause` command is meaningful only when it is within a procedure, not at top level. It suspends running of the procedure and tells you that you are pausing; you can then type instructions interactively. To indicate that you are in a pause and not a t top level, the prompt character changes to the name of the procedure you were in, followed by a question mark. During a pause, `BRK` does not work; the only way to return to top level during a pause is to run `throw "toplevel`.

The procedure may be resumed by typing `co`.


## stop

stop

`command`

The `stop` command stops the procedure that is running and returns control to the caller. This command is meaningful only when it is within a procedure—not at top level. Note that a procedure containing `stop` is a command. Compare `stop` with `output`.


## wait

wait _integer_

`command`

`wait` tells Logo to wait for _integer_ 10ths of a second.


## catch

catch _name_ _list_

`command`

`catch` runs _list_. If a `throw` _name_ command is called while _list_ is run, control returns to the first statement after the `catch`. The _name_ is used to match up a `throw` with a `catch`. For instance, `catch "chair [whatever]` catches a `throw "chair` but not a `throw "table`.

There is one special case. `catch "error` catches an error that would otherwise print an error message and return to top level. If an error is caught, the message that Logo would normally print isn't printed. See the explanation of `error` in this section to find out how to tell what the error was.


## error

error

`operation`

`error` outputs a four-element list containing information about the most recent error that has not had a message printed or output by `error`. If there was no such error, `error` outputs the empty list. The elements in the list are

- a unique number identifying the error
- a message explaining the error
- the name of the primitive causing the error, if any
- the name of the procedure within which the error occurred (the empty list, if top level).

Logo runs `throw "error [whenever]` an error occurs during the execution of a procedure. Control passes to top level unless a `catch "error` has been run. When an error is caught in this way, no error message is printed, and you can design your own.

Refer to the reference document [Error Messages](./Error_Messages.md) for a complete list of error messages and their meanings.


## go

go _word_

`command`

The `go` command transfers control to the instruction following `label` _word_ in the same procedure.


## label

label _word_

`command`

The `label` command itself does nothing. However, a `go` _word_ passes control to the instruction following it. Note that _word_ must always be a literal word (that is, it must be preceded by a quotation mark).


## repeat

repeat _integer_ _list_

`command`

`repeat` runs _list_ _integer_ times. An error occurs if _integer_ is negative.


## run

run _list_

`command` or `operation`

The `run` command runs _list_ as if typed in directly. If _list_ is an operation, then `run` outputs whatever _list_ outputs.


## throw

throw _name_

`command`

The `throw` command is meaningful only within the range of the `catch` command. An error occurs if no corresponding `catch` name is found. `throw "toplevel` returns control to top level. Contrast with `stop`.

See `catch`. 



# Managing your Workspace {#manage-workspace}

## nodes

nodes

`operation`

`nodes` outputs the number of free nodes. This gives you an idea of how much space you have in your workspace for procedures, variables, properties, and the running of procedures. `nodes` is most useful if run immediately after `recycle`.


## recycle

recycle

`command`

The `recycle` command frees up as many nodes as possible, performing what is called a garbage collection. When you don't use `recycle`, garbage collections happen automatically whenever necessary, but each one takes at least one second. Running `recycle` before a time-dependent activity prevents the automatic garbage collector from slowing things down at an awkward time.


## po

po _name_
po _list_

`command`

The `po` (for print out) command prints the definition(s) of the named procedure(s).


## poall

poall

`command`

The `poall` (for print out all) command prints the definition of every procedure and the value of every variable in the workspace.


## pon

pon _name_
pon _list_

`command`

`pon` (for print out name) prints the name and value of the named variable(s).


## pons

pons

`command`

`pons` (for print out names) prints the name and value of every variable in the workspace.


## pops

pops

`command`

`pops` (for print out procedures) prints the definition of every
procedure in the workspace. See `bury` for exceptions.


## pot

pot _name_
pot _list_

`command`

The `pot` (for print out title) command prints the title line of the named procedure(s) in the workspace.


## pots

pots

`command`

`pots` (for print out titles) prints the title line of every procedure in the workspace. See `bury` for exceptions.


## erall

erall

`command`

`erall` erases all procedures, variables, and properties from the workspace. See `bury` for exceptions.


## erase (er)

erase _name_
erase _list_
er _name_
er _list_

`command`

The `erase` command erases the named procedure(s) from the workspace.


## ern

ern _name_
ern _list_

`command`

The `ern` (for erase name) command erases the named variable(s) from the workspace.


## erns

erns

`command`

`erns` (for erase names) erases all variables from the workspace. See `bury` for exceptions.


## erps

erps

`command`

The `erps` (for erase procedures) command erases all procedures from the workspace. See `bury` for exceptions.


## bury 

bury _name_
bury _list_

`command`

The `bury` command buries the procedure(s) in its input. Certain commands (`erall`, `erps`, `poall`, `pops`, `pots`, and `save`) act on everything in the workspace except procedures and names that are buried. 


## buryall 

buryall

`command`

The `buryall` command buries all the procedures and variable names in the workspace. 

Once `buryall` is run, there are no procedure titles or names visible. 


## buryname

buryname _name_
buryname _list_

`command`

`buryname` buries the variable name(s) in its input.


## unbury 

unbury _name_
unbury _list_

`command`

The `unbury` command unburies the named procedure(s). 


## unburyall 

unburyall

`command`

`unburyall` unburies all procedures and variable names that are currently buried in the workspace. 

Once `unburyall` is run, the procedures and variable names are visible. 


## unburyname 

unburyname _name_
unburyname _list_

`command`

`unburyname` unburies the variable name(s) in its input. 




# File Management {#file-management}

## files

files
(files _ext_)

`operation`

Outputs a list of file names in the currect directory. If _ext_ is present, the file names are limited to those with the _ext_ extension. If  is "*" then all files are output.


## directories

directories

`operation`

Outputs a list of directory names in the current directory.


## chdir

chdir _pathname_

`command`

Stands for change directory. Changes the current directory name to _pathname_. 


## currentdir

currentdir

`operation`

Stands for current directory. Outputs the current directory that was set with `chdir`.


## erfile

erfile _pathname_

`command`

Stands for erase file. Erases any type of file. The input must be the name of a file in the current directory or a full pathname.


## erdir

erdir _pathname_

`command`

Stands for erase directory. Erases a directory. The directory must be empty or this command will result in an error.


## catalog

catalog

`command`

Prints a list of files and directories in the current directory. Directories have the slash "`/`" character appended to their pathname.


# Variables

## local

local _name_
local _list_

`command`

The `local` command makes its input(s) local to the procedure within which the `local` occurs. A local variable is accessible only to that procedure and to procedures it calls; in this regard it resembles inputs to the procedure.


## make

make _name_ _object_

`command`

The `make` command puts _object_ in _name_'s container, that is, it gives the variable name the value object.


## name

name _object_ _name_

`command`

The `name` command puts _object_ in _name_'s container, that is, it gives the variable name the value object.

`name` is equivalent to `make` with the order of the inputs reversed. Thus `name "welder "job` has the same effect as `make "job "welder`.


## name? (namep)

name? _word_
namep _word_

`operation`

`name?` outputs `true` if _word_ has a value, that is, if `:word` exists; it outputs `false` otherwise.


## thing

thing _name_

`operation`

`thing` outputs the thing in the container _name_, that is, the value of the variable _name_. `thing "any` is equivalent to `:any`.




# Debugging Programs

## step

step _name_
step _list_

`command`

The `step` command takes the procedure indicated by _name_ or _list_ as input and lets you run them line by line. `step` pauses at each line of execution and continues only when you press any key on the keyboard.


## trace

trace _name_
trace _list_

`command`

The `trace` command takes the procedures indicated by _name_ or _list_ as input and causes them to print tracing information when executed. It does not interrupt the execution of the procedure, but allows you to see the depth of the procedure stack during execution. `trace` is useful in understanding recursive procedures or complex programs with many subprocedures.


## unstep

unstep _name_
unstep _list_

`command`

`unstep` restores the procedure(s) indicated by _name_ or _list_ back to their original states. After you step through a procedure (with `step`), you must use `unstep` so that it will execute normally again.


## untrace

untrace _name_
untrace _list_

`command`

`untrace` stops the tracing of procedure _name_ and causes it to execute normally again.



# Logical Operations {#logical-operations}

## and

and _predicate1_ _predicate2_
(and _predicate1_ _predicate2_ _predicate3_ ...)

`operation`

Outputs `TRUE` if all of its inputs are `TRUE`.


## not

not _predicate_

`operation`

Outputs TRUE if _predicate_ is FALSE.
Outputs FASLE if _predicate_ is TRUE.


## or

or _predicate1_ _predicate2_
(or _predicate1_ _predicate2_ _predicate3_ ...)

`operation`

Outputs `TRUE` if any of its inputs are `TRUE`.



# Modifying Procedures Under Program Control

## copydef

copydef _name_ _newname_

`command`

`copydef` copies the definition of _name_, making it the definition of _newname_ as well.


## define

define _name_ _list_

`command`

`define` makes list the definition of the procedure _name_. The first element of _list_ is a list of the inputs to _name_, with no colon (`:`) before the names.

If _name_ has no inputs, this must be the empty list. Each subsequent element is a list consisting of one line of the procedure definition. (This list does not contain `end`, because `end` is not part of the procedure definition.)

The second input to `define` has the same form as the output from `text`. `define` can redefine an existing procedure.


## defined? (definedp)

defined? _word_
definedp _word_

`operation`

`defined?` outputs `true` if word is the name of a user-defined procedure, `false` otherwise.


## primative? (primativep)

primitive? _name_
primitivep _name_

`operation`

`primitivep` outputs `true` if name is the name of a primitive, `false` otherwise.


## text

text _name_

`operation`

The `text` primitive outputs the definition of _name_ as a list of lists, suitable for input to `define`.


# The Outside World {#outside-world}

## keyp

keyp

`operation`

`keyp` outputs `true` if there is at least one character waiting to be read—that is, one that has been typed on the keyboard and not yet picked up by `readchar` or `readlist`. `keyp` outputs `false` if there are no such characters.


## readchar (rc)

readchar
rc

`operation`

`readchar` outputs the first character typed at the keyboard or read from the current file. If you are reading from the keyboard and no character is waiting to be read, `readchar` waits until you type something.

`readchar` does not output a character if you are reading from a file and the end-of-file position is reached. In this case, `readchar` outputs an empty list. Note that `readchar` from the keyboard does not echo what you type on the display.

If you are reading from the keyboard, you can set the high bit of the character being read by holding down either `Alt` key a you type the character. Setting the high bit adds 128 to the character.


## readchars (rcs)

readchars _integer_
rcs _integer_

`operation`

The `readchars` operation outputs the first _integer_ number of characters typed at the keyboard or read from the current file. If you are reading from the keyboard and no characters arwaiting to be read, `readchars` waits for you to type something.

If you are reading from a file and the end-of-file position reached before _integer_ characters are read, `readchars` outputs the characters read up to that point. If the end-of-file was reached before `readchars` was called, `readchars` outputs an empty list.

Note that `readchars` from the keyboard does not echo what you type on the display.

Remember that a carriage return is read as a character.

If you are reading from the keyboard, you can set the high bit of the character being read by holding down either `Alt` key as you type the character. Setting the high bit adds 128 to character.


## readlist (rl)

EADLIST (RL)

`operation`

The `readlist` operation reads a line of information from the current file and outputs the information in the form of a list. Normally, the source is the keyboard, where you type in information followed by a carriage return. This information is echoed on the screen. The command `setread` allows you to read from other files.

If you are reading from a file where the end-of-file position has already been reached, `readlist` outputs the empty word.


## readword (rw)

readword
rw

`operation`

`readword` reads a line of information from the current file and outputs it as a word. Normally, the source is the keyboard and `readword` waits for you to type and press `ENTER`. What you type is echoed on the display. If you press `ENTER` before typing a word, `readword` outputs an empty word.

If you use `readword` from a file, `readword` reads characters until it reaches a carriage return, and outputs those characters as a word. The next character to be read is the one after the cariage return. When the end-of-file position is reached, `readword` outputs an empty list.

See `readlist`, `readchar`, `readchars`, and `setread`.


## print (pr)

print _object_
(print _object1_ _object2_ ...)
pr _object_
(pr _object1_ _object2_ ...)

`command`

The `print` command prints its inputs followed by a carriage return on the display, unless the destination has been changed by `setwrite`. The outermost brackets of lists are not printed.

Compare with `type` and `show`.


## show

show _object_

`command`

The `show` command prints _object_ followed by a carriage return on the display, unless the destination has been changed by `setwrite`. If object is a list, Logo leaves brackets around it.

Compare with `type` and `print`.


## type

type _object_
(type _object1_ _object2_ ...)

`command`

The `type` command prints its inputs without a carriage return on the display, unless the destination has been changed by `setwrite`. The outermost brackets of lists are not printed.

Compare with `print` and `show`.



# Property Lists {#property-lists}

## erprops

erprops

`command`

`erprops` (for erase properties) erases all properties from the workspace. To check which property lists are currently in the workspace, use `pps`. Use `remprop` to remove properties one at a time from the workspace.


## gprop

gprop _name_ _property_

`operation`

`gprop` (for get property) outputs the value of _property_ of _name_. If there is no such property, `gprop` outputs the empty list.


## plist

plist _name_

`operation`

`plist` outputs the property list associated with _name_. This is a list of property names paired with their values, in the form `[prop1 vall prop2 val2 ...]`.


## pprop

pprop _name_ _property_ _object_

`command`

The `pprop` (for put property) command gives _name_ _property_ with value _object_. Note that `erall` erases procedures, variables, and properties. Use `remprop` to erase properties one at a time or `erprops` to erase them all at once.


## pps

pps

`command`

The `pps` (for print properties) command prints the property lists of everything in the workspace.

## remprop

remprop _name_ _property_

`command`


The `remprop` (for remove property) command removes _property_ from the property list of _name_.

See `pprop` and `gprop`.


---