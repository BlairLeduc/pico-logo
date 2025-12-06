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

### How to Think about Procedures You Define and their Inputs

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

Words can contain any character and this includes alphanumeric characters, delimiters, tabs, and punctuation. Delimiters must be escaped by a backslash "`\`" character.

## Delimiters and Spacing

A word is usually _delimited_ only by spaces (a tab character is not considered a space): That is, there is a space before the word and a space after the word; they set the word off from the rest of line. There are a few other delimiting characters:

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



