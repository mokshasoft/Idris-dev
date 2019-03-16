module Main

import System.Concurrency.Channels

data Message = Add Int Int

adder : IO ()
adder = do
    Just chSender <- listen 1 | Nothing => adder
    Just msg <- unsafeRecv Message chSender | Nothing => adder
    case msg of
        Add x y => do
            ok <- unsafeSend chSender (x + y)
            adder

sendAdd : Channel -> Int -> Int -> IO Int
sendAdd ch i1 i2 = do
    ok <- unsafeSend ch (Add i1 i2)
    Just sum <- unsafeRecv Int ch | Nothing => pure 0
    pure sum

main : IO ()
main = do
    Just idAdder <- spawn adder | Nothing => putStrLn "spawn failed"
    Just ch <- connect idAdder | Nothing => putStrLn "connect failed"
    sum <- sendAdd ch 1 1
    putStrLn $ show sum
