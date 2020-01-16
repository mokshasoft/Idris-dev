module Main

import System.Concurrency.Channels

source : Int -> Channel -> IO ()
source stopNum to = do
    putStrLn "start source"
    source' 0 stopNum to
  where
    source' : Int -> Int -> Channel -> IO ()
    source' counter stopNum to = do
        putStrLn $ "Send number " ++ (show counter)
        _ <- unsafeSend to counter
        if counter > stopNum
            then pure ()
            else source' (counter + 1) stopNum to

{-
pipe : Int -> Channel -> IO ()
pipe stopNum to = do
    Just ch <- listen 1 | Nothing => pipe stopNum to
    Just msg <- unsafeRecv Int ch | Nothing => pipe stopNum to
    if msg > stopNum
        then stopThread
        else do
            _ <- unsafeSend to msg
            pipe stopNum to
-}

sink : Int -> IO ()
sink stopNum = do
    putStrLn "start sink"
    sink' stopNum
  where
    sink' : Int -> IO ()
    sink' stopNum = do
        Just ch <- listen 1 | Nothing => sink' stopNum
        Just msg <- unsafeRecv Int ch | Nothing => sink' stopNum
        putStrLn $ "Got number " ++ (show msg)
        if msg > stopNum
            then stopThread
            else do
                putStrLn $ show msg
                sink' stopNum

main : IO ()
main = do
    let stopNum = 100
    Just idSink <- spawn (sink stopNum) | Nothing => putStrLn "spawning sink failed"
    Just ch <- connect idSink | Nothing => putStrLn "connect to sink failed"
    source stopNum ch
    putStrLn "done"
