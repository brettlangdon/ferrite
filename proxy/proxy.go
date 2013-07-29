package proxy

import (
	"bitbucket.org/ww/cabinet"
	"fmt"
	"github.com/brettlangdon/ferrite/common"
	"io/ioutil"
	"net/http"
	"os"
	"time"
)

func CallProxy(url string, db *cabinet.KCDB) {
	response, err := http.Get(url)
	if err == nil {
		defer response.Body.Close()
		result, err := ioutil.ReadAll(response.Body)
		if err == nil {
			now := int32(time.Now().Unix())
			ttl := int32(*common.TTL)
			result = []byte(fmt.Sprintf("%d:%s", now+ttl, result))
			db.Set([]byte(url), result)
		} else {
			fmt.Fprintf(os.Stderr, "Error Reading Response for \"%s\": %s\r\n", url, err.Error())
		}
	} else {
		fmt.Fprintf(os.Stderr, "Error Retrieving Proxy Results for \"%s\": %s\r\n", url, err.Error())
	}
}
