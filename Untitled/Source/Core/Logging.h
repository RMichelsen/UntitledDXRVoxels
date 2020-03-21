#pragma once

#ifndef NDEBUG
#define UNTITLED_ASSERT(x) assert(x)
#define UNTITLED_LOG_INFO(...) ( printf("[INFO]: ") , printf(__VA_ARGS__) )
#define UNTITLED_LOG_WARN(...) ( printf("[WARN]: ") , printf(__VA_ARGS__) )
#define UNTITLED_LOG_ERROR(...) ( printf("[ERROR]: ") , printf(__VA_ARGS__) , UNTITLED_ASSERT(false) )
#else	
#define UNTITLED_ASSERT(x) do { (void)sizeof(x);} while (0)
#define UNTITLED_LOG_INFO(...) do { (void)sizeof(__VA_ARGS__);} while (0)
#define UNTITLED_LOG_WARN(...) do { (void)sizeof(__VA_ARGS__);} while (0)
#define UNTITLED_LOG_ERROR(...) do { (void)sizeof(__VA_ARGS__);} while (0)
#endif