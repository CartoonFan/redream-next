test_not:
  # REGISTER_IN r0 0xf0f0f0f0
  not r0, r1
  rts 
  nop
  # REGISTER_OUT r1 0x0f0f0f0f
