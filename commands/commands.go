package commands

import (
	"bitbucket.org/ww/cabinet"
	"bytes"
	"fmt"
	"github.com/brettlangdon/ferrite/common"
	"github.com/brettlangdon/ferrite/proxy"
	"net/url"
	"os"
	"strconv"
	"strings"
	"sync/atomic"
	"time"
)

func ParseCommand(request string) (string, []string) {
	request = strings.ToLower(request)
	tokens := strings.Split(strings.Trim(request, " \r\n"), " ")
	var command string
	if len(tokens) > 0 {
		command = tokens[0]
	}
	return command, tokens
}

func HandleCommand(command string, tokens []string, db *cabinet.KCDB) string {
	var result string
	switch command {
	case "get":
		result = HandleGet(tokens[1:], db)
		break
	case "stats":
		result = HandleStats(db)
		break
	case "flush_all":
		result = HandleFlushAll(db)
		break
	case "delete":
		result = HandleDelete(tokens[1:], db)
		break
	case "version":
		result = HandleVersion()
		break
	case "help":
		result = HandleHelp()
		break
	default:
		result = "ERROR\r\n"
		break
	}

	return result
}

func HandleGet(tokens []string, db *cabinet.KCDB) string {
	var out string
	if len(tokens) >= 1 {
		key := tokens[0]
		if strings.HasPrefix(key, "http%3a%2f%2f") || strings.HasPrefix(key, "https%3a%2f%2f") {
			key, _ = url.QueryUnescape(key)
		}
		result, err := db.Get([]byte(key))
		if err != nil {
			atomic.AddInt32(&common.MISSES, 1)
			err := db.Set([]byte(key), []byte("0"))
			if err == nil {
				go proxy.CallProxy(key, db)
			} else {
				fmt.Fprintf(os.Stderr, "Error Setting Key \"%s\" Value \"0\": $s\r\n", key, err.Error())
			}
			out = "END\r\n"
		} else if bytes.Equal(result, []byte("0")) {
			atomic.AddInt32(&common.MISSES, 1)
			out = "END\r\n"
		} else {
			parts := strings.SplitN(string(result), ":", 2)
			var expire int64 = 0
			if len(parts) == 2 {
				expire, err = strconv.ParseInt(parts[0], 10, 32)
				if err != nil {
					expire = 0
					fmt.Fprintf(os.Stderr, "Error Parsing Expiration \"%s\": %s\r\n", parts[0], err.Error())
				}
				result = []byte(parts[1])
			}
			now := int64(time.Now().Unix())
			if expire > 0 && expire < now {
				err := db.Set([]byte(key), []byte("0"))
				if err == nil {
					go proxy.CallProxy(key, db)
				} else {
					fmt.Fprintf(os.Stderr, "Error Setting Key \"%s\" Value \"0\": $s\r\n", key, err.Error())
				}
				atomic.AddInt32(&common.MISSES, 1)
				out = "END\r\n"
			} else {
				atomic.AddInt32(&common.HITS, 1)
				out = fmt.Sprintf("VALUE %s 0 %d\r\n%s\r\nEND\r\n", key, len(result), result)
			}
		}
	} else {
		out = "ERROR\r\n"
	}
	return out
}

func HandleStats(db *cabinet.KCDB) string {
	hits := atomic.LoadInt32(&common.HITS)
	misses := atomic.LoadInt32(&common.MISSES)
	connections := atomic.LoadInt32(&common.CONNECTIONS)
	var hit_ratio float32 = 0
	if hits > 0 {
		hit_ratio = float32(hits) / float32(hits+misses)
	}

	db_stats := ""
	status, err := db.Status()
	if err == nil {
		lines := strings.Split(status, "\n")
		for _, line := range lines {
			parts := strings.SplitN(strings.Trim(line, "\r\n"), "\t", 2)
			if len(parts) == 2 {
				db_stats += fmt.Sprintf("STAT %s %s\r\n", parts[0], parts[1])
			}
		}
	} else {
		fmt.Fprintf(os.Stderr, "Error Retrieving DB Status: %s\r\n", err.Error())
	}
	return fmt.Sprintf("STAT hits %d\r\n"+
		"STAT misses %d\r\n"+
		"STAT hit_ratio %1.4f\r\n"+
		"STAT connections %d\r\n"+
		"%s"+
		"END\r\n",
		hits, misses, hit_ratio, connections, db_stats,
	)
}

func HandleFlushAll(db *cabinet.KCDB) string {
	db.Clear()
	return "OK\r\n"
}

func HandleDelete(tokens []string, db *cabinet.KCDB) string {
	if len(tokens) <= 0 {
		return "ERROR\r\n"
	}

	key := tokens[0]
	err := db.Remove([]byte(key))
	if err != nil {
		return "NOT_FOUND\r\n"
	}
	return "DELETED\r\n"
}

func HandleVersion() string {
	return fmt.Sprintf("VERSION %s\r\n", common.VERSION)
}

func HandleHelp() string {
	return "COMMAND GET <KEY> - retrieve <KEY> from the cache\r\n" +
		"COMMAND FLUSH_ALL - remove all records from the cache\r\n" +
		"COMMAND DELETE <KEY> - remove <KEY> from the cache\r\n" +
		"COMMAND VERSION - display the application version\r\n" +
		"COMMAND STATS - display application statistics\r\n" +
		"COMMAND QUIT - close the connection\r\n" +
		"COMMAND EXIT - same as QUIT\r\n" +
		"COMMAND HELP - display this information\r\n" +
		"END\r\n"
}
