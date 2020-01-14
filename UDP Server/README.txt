Name: Chad Lewis
Identikey: chle1876

In this assignment I have implemented a non-reliable transfer UDP connection between a client and a server.

To run the application:
Open Two Consoles, 1  for the server 1 for the client.

On one console:
  Navigate to server/ directory.
  enter command "make"
  enter command "./server [port_number]" (I use  8888)
On the other console
  Navigate to client/ directory.
  enter command "make"
  enter command "hostname -i", remember this  number we  will refer to as IP.
  enter command "./client [IP] [port_number]", ensure this number is  the same as above.


On the client the possible commands are:
get     [file_name] (Will pull a file from the server into your client/ directory)
put     [file_name] (Will push a file from the client to the server/ directory)
delete  [file_name] (Will delete a file from the server/ directory)
ls                  (Will list all files in the server/ directory)
exit                (Will end the server cleanly)
