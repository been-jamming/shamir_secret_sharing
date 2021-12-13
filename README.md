# Shamir Secret Sharing
An implementation of Shamir's secret sharing scheme. Splits a file into several pieces of the same size with a specified number of pieces required to recover the file.

## Command line options ##
Option   | Function
-------- |---------
-r       |Read given shares and output the secret
-w       |Read given secret and output shares
-s [num] |When -w specified, gives the number of shares to write
-n [num] |When -w specified, gives the number of shares required to recover the secret
[file]   |When -w specified, gives the file to be split into shares. When -r specified, gives as input a share to read
-o [file]|When -r specified, gives the file name of the output
## Example Usage ##

```./shamir -w -s 5 -n 3 test.txt```

Writes 5 shares called `0_test.txt`, `1_test.txt`, `2_test.txt`, `3_test.txt`, and `4_test.txt`. Any 3 of these shares will recover the original file. For example, one could execute:

```./shamir -r 1_test.txt 0_test.txt 4_test.txt -o test.txt ```

to get back the original file.
