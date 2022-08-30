package api

import (
	"errors"
	"fmt"
	"io/ioutil"
	"net/url"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
)

func get(ctx *gin.Context, name string, required bool, defaultValue interface{}, errs *[]error) interface{} {

	var val string

	if ctx.Request.Method == "POST" {
		val = ctx.PostForm(name)

		// if POST binary content, get from query
		if val == "" {
			val = ctx.Query(name)
		}

	} else {
		val = ctx.Query(name)
	}

	if required && val == "" {
		toError(errs, name+" is required")
		return defaultValue
	} else if val == "" {
		return defaultValue
	}

	val, err := url.QueryUnescape(val)

	if err != nil {
		toError(errs, err.Error())
	}

	return val
}

func getString(ctx *gin.Context, name string, required bool, defaultValue interface{}, errs *[]error) string {
	val := get(ctx, name, required, defaultValue, errs)
	return toString(val)
}

func getInt(ctx *gin.Context, name string, required bool, defaultValue interface{}, errs *[]error) int {
	val := get(ctx, name, required, defaultValue, errs)
	c := toI32(val)
	return int(c)
}

func getBytes(ctx *gin.Context, errs *[]error) []byte {
	ByteBody, err := ioutil.ReadAll(ctx.Request.Body)

	if err != nil {
		toError(errs, err.Error())
		return nil
	}

	return ByteBody
}

func toError(errs *[]error, msg string) {
	*errs = append(*errs, errors.New(msg))
}

func hasError(errs []error) error {

	for _, err := range errs {
		if err != nil {
			return err
		}
	}

	return nil
}

func createError(err error) map[string]interface{} {

	errs := make(map[string]interface{})
	errs["error"] = err.Error()

	return errs
}

func toTime(end time.Duration) string {
	return fmt.Sprintf("%v", end)
}

func toString(t interface{}) string {
	return fmt.Sprintf("%+v", t)
}

func toI32(t interface{}) int32 {
	s := toString(t)
	i, err := strconv.Atoi(s)

	if err != nil {
		return 0
	}

	return int32(i)
}
