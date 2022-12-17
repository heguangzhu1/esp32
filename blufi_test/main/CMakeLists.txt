idf_component_register(SRCS 
							"tcp_server.c"
							"simple_ota_example.c"
                    INCLUDE_DIRS "."
					EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)
