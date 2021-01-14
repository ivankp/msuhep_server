
 1. Create the main socket using bind, etc.
 2. Put the main socket into epoll
 3. In the main loop:

      1. Call epoll_wait for activity on any FD in epoll.
      2. On activity:
         a) If the file is the main socket, then call accept and insert
            the client FD into epoll. Go back to the top of the main loop.
         b) If the file is not the main socket, spin up a worker thread to
            handle the client FD. (Ideally, put the FD into a queue of FDs
            for threads to handle.)

 4. In each worker thread:
      1. Use condition variables to wait for there to be a FD in the queue
         of FDs.
      2. Handle everything for the activity on the client FD.
      3. When done (if the client FD wasn't closed) then re-insert the FD
         into epoll.
      4. On error/hangup, etc, close the client socket

