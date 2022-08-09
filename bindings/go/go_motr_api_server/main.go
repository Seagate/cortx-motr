package main

import (
	"go_motr_api_server/v2/api"
	"go_motr_api_server/v2/app"

	"github.com/DeanThompson/ginpprof"
	"github.com/gin-contrib/cors"
	"github.com/gin-contrib/gzip"
	"github.com/gin-gonic/gin"
)

func main() {

	//gin.SetMode(gin.ReleaseMode)

	router := gin.Default()
	router.Use(cors.Default())
	router.Use(gzip.Gzip(gzip.DefaultCompression))

	//pprof.Register(router, nil)
	ginpprof.Wrapper(router)

	application := router.Group("api")

	application.GET("status", api.Status)

	kv := application.Group("kv")
	{
		kv.GET("get", api.GetKey)
		kv.POST("put", api.PutKeyValue)
		kv.POST("delete", api.DeleteKey)
	}

	obj := application.Group("object")
	{
		obj.GET("read", api.Read)
		obj.POST("write", api.Write)
	}

	router.Run(app.Conf.Address)
}
