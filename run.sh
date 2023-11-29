arg_port=$1

bin_dir="bin"
output_bin_name="simple-http-api-server"

# Ensure that the bin directory for compilation output exists
if [ ! -d "./$bin_dir" ]; then
    mkdir "./$bin_dir"
fi

# Compile the server code
gcc "./src/main.c" -o "./$bin_dir/$output_bin_name"

# Run the server from the compiled binary
./$bin_dir/$output_bin_name $arg_port
