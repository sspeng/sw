sw5cc -slave -c  func.c -g
sw5CC -host -c  main.cpp -g
mpiCC main.o func.o -o xx -g
bsub -I -n 1 -cgsp 64 ./xx
