# Cleaning and Organizing the Workspace 

This section discusses the primitives that you use to manage your workspace effectively. The primitives for doing this are 

- `BURY`
- `BURYALL`
- `BURYNAME`
- `UNBURY`
- `UNBURYALL`
- `UNBURYNAME`

## BURY 

`BURY name(list)`

The BURY command buries the procedure(s) in its input. Certain commands (`ERALL`, `ERPS`, `POALL`, `POPS`, `POTS`, and `SAVE`) act on everything in the workspace except procedures and names that are buried. 

Example:

`SAVE "GOODSTUFF` saves the whole workspace in the file `GOODSTUFF` except procedures and names that are buried. 

## BURYALL 

`BURYALL`

The `BURYALL` command buries all the procedures and variable names in the workspace. 

Example: 
```logo
? POTS 
TO POLY :SIDE 
TO LENGTH : OBJ 
TO GREET 
TO SPI :SIDE :ANGLE :INC 
?PONS 
MAKE "ANIMAL "AARDVARK
MAKE "LENGTH 3.98
MAKE "MYNAME "STEVE
?BURYALL 
?POTS 

?PONS 

?
```

Once `BURYALL` is run, there are no procedure titles or names visible. 

## BURYNAME

`BURYNAME name(list)`

`BURYNAME` buries the variable name(s) in its input.

Example: 
```logo
?PONS 
MAKE "ANIMAL "AARDVARK 
MAKE "LENGTH 3.98 
MAKE "MYNAME "STEVE 
?BURYNAME "MYNAME 
?PONS 
MAKE "ANIMAL "AARDVARK 
MAKE" "LENGTH 3.98 
```

## UNBURY 

`UNBURY name(list)`

The `UNBURY` command unburies the named procedure(s). 

## UNBURYALL 

`UNBURYALL`

`UNBURYALL` unburies all procedures and variable names that are currently buried in the workspace. 

Example: 
``` logo
?POTS 
?PONS
```

There are no procedures or variable names printed. 

```logo
?UNBURYALL 
?POTS 
TO POLY :SIDE : ANGLE 
TO LENGTH : OBJ 
TO GREET 
TO SP1 :SIDE :ANGLE :INC 
?PONS 
MAKE "ANIMAL "AARDVARK 
MAKE "LENGTH 3.98 
MAKE "MYNAME "STEVE 
```

Once `UNBURYALL` is run, the procedures and variable names are visible. 

## UNBURYNAME 

`UNBURYNAME name(list)`

`UNBURYNAME` unburies the variable name(s) in its input. 

Example: 
```logo
?PONS 

? 
```

There are no variables visible. 

```logo
?UNBURYNAME [LENGTH NOUNS] 
?PONS 
MAKE "LENGTH 3.98 
MAKE "NOUNS [COMPUTERS HOUSES BEDS CHAIRS TV STEREO] 
```
