print("Starting faulty Lua script...")
local secret_table = nil
-- Deliberate runtime nil-dereference bug:
print(secret_table.kernel_password)
print("This line should never run!")
