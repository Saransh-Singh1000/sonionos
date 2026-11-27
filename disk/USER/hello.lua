-- Read a line of text
io.write("What is your name? ")
local name = io.read("l")
print("Hello, " .. name .. "!")

-- Read a number
io.write("Enter a number: ")
local n = io.read("n")
io.read("l")  -- consume the leftover newline
print("Your number squared is", n * n)

-- Simple loop: keep asking until the user types "quit"
while true do
  io.write("sonion> ")
  local line = io.read("l")
  if line == nil or line == "quit" then
    print("bye!")
    break
  end
  print("you typed:", line, "(" .. #line .. " characters)")
end

-- Tiny calculator
io.write("Enter two numbers separated by a space: ")
local x, y = io.read("n", "n")
print("sum =", x + y)
print("product =", x * y)
print(10/3)