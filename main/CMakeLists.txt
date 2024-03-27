idf_component_register(SRCS 
				main.c 
				app_peripherals.c
                    	
			INCLUDE_DIRS 
				include
			
			EMBED_TXTFILES
				../certs/client_cert.pem
				../certs/client_key.pem
			
			)