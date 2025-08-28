Cody Wiggins
Professor Mittal
CS 4348.004
5/02/25

Files included in this folder: pipegrep.cpp (main program), pipegrepWithTimers.cpp (altered program for experiments to show thread and program times), pipegrepTimeAverage.cpp (altered program for experiments to show program time average after a 500 executions in different buffer sizes), Makefile (ONLY makes executable for pipegrep.cpp), and this READ_ME.txt file.

This pipegrep file will search through the regular files and directories within the current directory for the given keyword and can filter the results based on UID, GID, and file size with variable size buffers. With the given make file, make will create the executable "pipegrep" and the usage of pipegrep is "pipegrep <buffsize> <filesize> <uid> <gid> <word>". 

1. I have the search function through the current directory and the recursive search through subdirectories working. This function assumes that there are no cycles in the directories it will search through. Any regular file will be read line by line excluding binary files, since the output of the binary files was illegible. The UID, GID, and file size filter works. The condition variables are safe for all the buffers with minimal operations in the critical sections. Also, the done token used is the tuple ("", "", -5) to signal the producer is done adding to the buffer.

2. The critical section in the add function in the Buffer object acquires the lock, then checks to see if the size of the queue is the max size. If it is, then the thread is blocked until notified with the notFull condition variable. If it isn't, then the item is pushed to the buffer, the lock is released, and then the notEmpty condition variable is notified. 

Upon the initial acquiring of the lock, all that is done is to check the size of the queue to ensure that it isn't full. This is to ensure that this thread isn't trying to check the queue size while another thread is in the process of adding or removing something to the buffer to get a proper current size of the queue. After the possible wait that the thread may have in which the lock is released, the only item in the critical section is adding the item to the back of the queue. This is to ensure that while adding the item to the queue no other thread is adding or removing from the queue, which could cause memory issues in the queue or could overwrite an item in the queue.

The critical section in the remove function in the Buffer object acquires the lock, then checks to see if the size of the queue is zero. If it is then the thread is blocked until notified with the notEmpty condition variable. If it isn't, then the item at the front of the queue is copied to the address passed as an argument to the remove function and then the front item is popped from the queue. Finally, the lock is released and the notFull condition variable is notified.

Upon the initial acquiring of the lock, all that is done is to check the size of the queue to ensure that it isn't empty. This is to ensure that this thread isn't trying to check the queue size while another thread is in the process of adding or removing something to the buffer to get a proper current size of the queue. After the possible wait that the thread may have in which the lock is released, the only 2 items in the critical section are placing the front of the queue in the passed reference and popping that item from the queue. These are both to ensure that no other thread will be affecting the item that is being copied from the queue while the current thread is copying and removing that item. Along with ensuring that the indexing of the queue stays in proper order and not allowing possible situations that would have this item being removed while one is being added and causing issues with an item being removed and being read simultaneously.

3. Listed below is the output of running the "pipegrepTimeAverage.cpp" with the same keyword in the same directory with 50 items in it executing 500 times each for the buffer sizes of 1, 5, 10, 15, 20, 25, 30, 35, 40, 50, and 60. With how many different factors can affect the result, this is only an estimate from timing the program 500 times in a nonconstant environment, but the results do mostly show a trend of being somewhat similar in their times after a buffer size of 1, with the outlier of 25 being a bit higher than the others. What does seem to show in the directory that I tested in, is that 40 and 50 seem to be where the program is most efficient in runtime, since there is a steady trend in the time decreasing and the times start increasing afterwards.

Average of Buffer Size 1: 239.126 ms
Average of Buffer Size 5: 194.270 ms
Average of Buffer Size 10: 192.698 ms
Average of Buffer Size 15: 191.164 ms
Average of Buffer Size 20: 191.180 ms
Average of Buffer Size 25: 193.968 ms
Average of Buffer Size 30: 191.062 ms
Average of Buffer Size 35: 192.136 ms
Average of Buffer Size 40: 190.332 ms
Average of Buffer Size 50: 190.282 ms
Average of Buffer Size 60: 201.816 ms


4. If there are n files in the checked directory and m average lines in the files to read, then stage 1 and 2 are O(n). Stage 3 has a rate of O(n*m). While stage 4 and 5 also have that growth rate in the worst case. Stage 3 has the most operations and 2 of them are O(m). Since it has the highest potential workload, which is corroborated in the output of the times each thread has in running the "pipegrepWithTimers.cpp" by the bottleneck that starts at thread 3 consistently, and it would be easy to separate this workload by having 2 consumers alternate and grab the filenames. I would add a thread to split the work of stage 3. I believe the best option would be to have both threads do all of the functions of stage 3, but alternate taking from buffer 2 and outputting to buffer 3. This would require counting both of the done tokens or making 2 separate valued done tokens to signify to stage four which thread is done, but that is an easy change to make and the monitor would allow for multiple producers and consumers.

5. The only bugs I could think of would be if the filepath or the text of the file have characters that cannot be read. Then unexpected output similar to if it were to read binary files would appear. This is particularly possible after the first 512 bytes of any file, since that is all that is tested in the check I have made to remove the reading of binary files. Also, if there are any cycles in the directories, then the first thread will loop infinitely.