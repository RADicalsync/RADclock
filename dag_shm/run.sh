# After DAG machine reboot - possibly needed to run
# dagconfig default
# make -C /sys/modules/dag load


# To start capture - after dag device loaded
# In order to run this program the DAG must be running the following process
# sudo dagconvert -d0 -o /tmp/shm_dag.erf  -r 1m --fnum 2



clang main.c ../radclock/ntp_auth.c -o shm_dag -lwolfssl -L/usr/local/lib/ -I /usr/src/sys -I /usr/local/include/
./shm_dag /tmp/shm_dag.erf 10.0.0.55

# Not needed function. Listing it as an alternative capture method for possible future use
# sudo dagsnap -d0 -o /tmp/shm_dag.erf

