```plantuml
@startuml

title Simple Obj Upload with two storage set cluster

participant "S3 Client" as client
box StorageSet1 (SS1) #FEFEFE
participant "S3 Server/N1" as N1
participant "ctrl-A/CL-1" as CL_1
participant "ctrl-B/CR-1" as CR_1
participant "N2" as N2 #red
participant "CL-2" as CL_2
participant "CR-2" as CR_2
participant "N3" as N3
participant "CL-3" as CL_3
participant "CR-3" as CR_3
end box
box StorageSet2 (SS2) #EEEEEE
participant "N4" as N4 #red
participant "CL-4" as CL_4
participant "CR-4" as CR_4
participant "N5" as N5
participant "CL-5" as CL_5
participant "CR-5" as CR_5
participant "N6" as N6
participant "CL-6" as CL_6
participant "CR-6" as CR_6
end box

client -> N1: PUT /bucket_name/object_name

== Lookup 1 : Global bucket ==
N1 -> N2: get_keyval(global_bucket_index,\n key = "bucket_name")
N2 --X N1: Error 
N1 -> N5: get_keyval(global_bucket_index,\n key = "bucket_name")
N5 --> N1: value = account_id of bucket owner

== Lookup 2 : Global Bucket ==
N1 -> N2: get_keyval(global_bucket_md_index,\n key = "account_id/bucket_name")
N2 --X N1: Error 
N1 -> N3: get_keyval(global_bucket_md_index,\n key = "account_id/bucket_name")
N3 --> N1: value = bucket metadata JSON

note left
   * Global lookup will have 1 + N-1 replication 
   * N total node in the system with __**Node2**__ in error
   * __**Any error in global lookup will needs to be retried with other nodes**__
end note

== Lookup 3 : Local Storage Set  ==
N1 -> N4: get_keyval(BUCKET_nnn_obj_index,\n key = "object_name");
N4 --X N1 : Error
N1 -> N5: get_keyval(BUCKET_nnn_obj_index,\n key = "object_name");
note left
   * KV in Local Storage Set will have 1 + M-1 replication 
   * M total node in the Storage System, with __**Node4**__ in error
   * __**Any error in local lookup will needs to be retried with other nodes in SS**__
end note
N5 --> N1: not found

== Write Operation ==
N1 -> N1: create_object
N1 --> N1: success (completed)

loop until all data is written
  N1 -> N4: Write data
  N1 -> N5: Write data
  N1 -> N6: Write data

  N5 --> N1: success
  N6 --> N1: success
  N4 --X N1: success
note left
   * One of the Write Data returns error (Node 4)
   * __**Return Success if Total-Error < #Parity-Unit **__
end note

end

== Local SS metadata write ==
N1 -> N4: put_keyval(BUCKET_nnn_obj_index,\n key = object_name, val = object_metadata)
N4 --X N1: ERROR

N1 -> N5: put_keyval(BUCKET_nnn_obj_index,\n key = object_name, val = object_metadata)
N5 --> N1: success (completed)

N1 -> N6: put_keyval(BUCKET_nnn_obj_index,\n key = object_name, val = object_metadata)
N6 --> N1: success (completed)

N1 --> client: 200 OK

@enduml
