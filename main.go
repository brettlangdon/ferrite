package main

import (
	"bitbucket.org/ww/cabinet"
	"flag"
	"fmt"
	"github.com/brettlangdon/ferrite/commands"
	"github.com/brettlangdon/ferrite/common"
	"net"
	"os"
	"os/signal"
	"sync/atomic"
)

func handler(client net.Conn, db *cabinet.KCDB) {
	atomic.AddInt32(&common.CONNECTIONS, 1)
	defer func() {
		client.Close()
		atomic.AddInt32(&common.CONNECTIONS, -1)
	}()
	buffer := make([]byte, 1024)
	for {
		n, err := client.Read(buffer)
		if err != nil {
			break
		}
		if n <= 0 {
			continue
		}

		command, tokens := commands.ParseCommand(string(buffer[:n]))
		if len(command) == 0 {
			continue
		}
		if command == "quit" || command == "exit" {
			break
		}
		response := commands.HandleCommand(command, tokens, db)
		_, err = client.Write([]byte(response))
	}
}

func main() {
	flag.Parse()

	fmt.Printf("Using TTL: %d (sec)\r\n", *common.TTL)
	fmt.Printf("Opening Cache: %s\r\n", *common.CACHE)
	db := cabinet.New()
	err := db.Open(*common.CACHE, cabinet.KCOWRITER|cabinet.KCOCREATE)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Could not open cache \"%s\": %s\r\n", *common.CACHE, err.Error())
		os.Exit(-1)
	}

	listener, err := net.Listen("tcp", *common.BIND)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Could not bind to \"%s\": %s\r\n", *common.BIND, err.Error())
		os.Exit(-1)
	}
	fmt.Printf("Listening at %s\t\n", *common.BIND)

	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt)
	go func() {
		for _ = range c {
			fmt.Println("")
			fmt.Printf("Shutting down ferrite\r\n")
			db.Close()
			db.Del()
			os.Exit(0)
		}
	}()

	for {
		client, err := listener.Accept()
		if err == nil {
			go handler(client, db)
		} else {
			fmt.Fprintf(os.Stderr, "Error Accepting Client Connection: %s\r\n", err.Error())
		}
	}
}
