# Hex value provided
hex_value = "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa08757365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2"

# Convert hex to binary data
binary_data = bytes.fromhex(hex_value)

# Specify the output file name
output_file = "empty.rdb"

# Write the binary data to the file
with open(output_file, "wb") as file:
    file.write(binary_data)

print(f"Binary file '{output_file}' has been created successfully.")

