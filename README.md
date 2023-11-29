# Simple HTTP API Server

A simple HTTP API server written in C.

## Running the server

There are instructions for starting the server below. After you have successfully started the server, you may navigate to `http://localhost:<PORT>/example.html` to see an example page.

### Linux

Run the Bash script to both compile and to run the server:

`./run.sh <PORT>`

The code is compiled into `bin` directory (the directory is created if it does not exist).

### Other operating systems

Currently, all development and testing is performed on Ubuntu Linux. Therefore there are no instructions for other operating systems.

## Development

### Naming conventions

- **Macros** All uppercase where words are separated by an underscore: `MAX_LEN` 
- **Constants** All uppercase where words are separated by an underscore: `MAX_LEN` 
- **Struct names** Upper camel case where abbreviations have all letters in lowercase except for the first one: `RequestBody`, `HttpRequest`
- **Functions** Snake case: `create_request_body`
- **Variables** Snake case: `response_body`