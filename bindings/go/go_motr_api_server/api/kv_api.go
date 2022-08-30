package api

import (
	"fmt"
	"go_motr_api_server/v2/motr"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
)

func Status(ctx *gin.Context) {

	json := make(map[string]interface{})
	json["status"] = "ok"

	ctx.JSON(http.StatusOK, json)
}

func GetKey(ctx *gin.Context) {

	start := time.Now()

	errs := &[]error{}

	indexID := getString(ctx, "index_id", true, nil, errs)
	key := getString(ctx, "key", true, nil, errs)

	if err := hasError(*errs); err != nil {
		ctx.AbortWithStatusJSON(http.StatusBadRequest, createError(err))
		return
	}

	value, err := motr.Get(indexID, key)

	if err != nil {
		ctx.AbortWithStatusJSON(http.StatusBadRequest, createError(err))
		return
	}

	end := time.Since(start)

	meta := make(map[string]interface{})
	meta["time"] = toTime(end)

	json := make(map[string]interface{})
	json["meta"] = meta
	json["index_id"] = indexID
	json["key"] = key
	json["value"] = value

	ctx.JSON(http.StatusOK, json)
}

func DeleteKey(ctx *gin.Context) {

	start := time.Now()

	errs := &[]error{}

	indexID := getString(ctx, "index_id", true, nil, errs)
	key := getString(ctx, "key", true, nil, errs)

	if err := hasError(*errs); err != nil {
		ctx.AbortWithStatusJSON(http.StatusBadRequest, createError(err))
		return
	}

	err := motr.Delete(indexID, key)

	if err != nil {
		ctx.AbortWithStatusJSON(http.StatusBadRequest, createError(err))
		return
	}

	end := time.Since(start)

	meta := make(map[string]interface{})
	meta["time"] = toTime(end)

	json := make(map[string]interface{})
	json["meta"] = meta
	json["index_id"] = indexID
	json["key"] = key

	ctx.JSON(http.StatusOK, json)
}

func PutKeyValue(ctx *gin.Context) {

	start := time.Now()

	errs := &[]error{}

	indexID := getString(ctx, "index_id", true, nil, errs)
	key := getString(ctx, "key", true, nil, errs)
	value := getString(ctx, "value", true, nil, errs)

	if err := hasError(*errs); err != nil {
		ctx.AbortWithStatusJSON(http.StatusBadRequest, createError(err))
		return
	}

	fmt.Println(indexID, key, value)

	err := motr.Put(indexID, key, value)

	if err != nil {
		ctx.AbortWithStatusJSON(http.StatusBadRequest, createError(err))
		return
	}

	end := time.Since(start)

	meta := make(map[string]interface{})
	meta["time"] = toTime(end)

	json := make(map[string]interface{})
	json["meta"] = meta
	json["index_id"] = indexID
	json["key"] = key
	json["value"] = value

	ctx.JSON(http.StatusOK, json)
}
