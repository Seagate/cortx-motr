package api

import (
	"bytes"
	"go_motr_api_server/v2/motr"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
)

func Write(ctx *gin.Context) {

	start := time.Now()

	errs := &[]error{}

	indexID := getString(ctx, "index_id", true, nil, errs)
	size := getInt(ctx, "size", true, nil, errs)
	byteBody := getBytes(ctx, errs)

	if err := hasError(*errs); err != nil {
		ctx.AbortWithStatusJSON(http.StatusBadRequest, createError(err))
		return
	}

	err := motr.Write("", indexID, uint64(size), bytes.NewReader(byteBody))

	if err != nil {
		ctx.AbortWithStatusJSON(http.StatusBadRequest, createError(err))
		return
	}

	end := time.Since(start)

	meta := make(map[string]interface{})
	meta["time"] = toTime(end)

	json := make(map[string]interface{})
	json["meta"] = meta
	json["indexID"] = indexID
	json["size"] = size

	ctx.JSON(http.StatusOK, json)
}

func Read(ctx *gin.Context) {

	start := time.Now()

	errs := &[]error{}

	indexID := getString(ctx, "index_id", true, nil, errs)
	size := getInt(ctx, "size", true, nil, errs)

	if err := hasError(*errs); err != nil {
		ctx.AbortWithStatusJSON(http.StatusBadRequest, createError(err))
		return
	}

	byteContent, err := motr.Read("", indexID, uint64(size))

	if err != nil {
		ctx.AbortWithStatusJSON(http.StatusBadRequest, createError(err))
		return
	}

	end := time.Since(start)

	ctx.Header("time", toTime(end))
	ctx.Data(http.StatusOK, "application/octet-stream", byteContent)
}
