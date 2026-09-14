# Bugs and stuff noticed while writing tests

## Bugs
Things that are definitely wrong and should be fixed:

## Oddities
Things that smell kind of bad, maybe have a look:
 - tString::Len() includes the trailing \0, so is one more than one would expect.
 - Worse, in the 0.2.9 branch Size() is the allocated capacity, on trunk it is the length without trailing \0.
    (We eliminated all uses of Size() in 0.2.9)

## TODOs
Tests we would like to have written, but could not yet:
