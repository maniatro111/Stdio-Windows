#define DLL_EXPORTS
#include "so_stdio.h"
#include <windows.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/*#include <fileapi.h>*/

struct _so_file {
	/* variable for storing the file descriptor */
	HANDLE fd;
	/* variable for storing the pid of the child proccess ( if needed) */
	PROCESS_INFORMATION pid;
	/* variable for storing the data read from a file or data that needs */
	/* to be written to a file */
	unsigned char *buffer;
	/* variable used to know the next character that needs to be written */
	/* from the buffer */
	int start;
	/* variable used to know the number of bytes read from the file */
	int len_read;
	/* variable used to know if there was an error during operating the file
	 */
	int error;
	/* variable used to know the last operation that was made on the */
	/* file descriptor ( read/write/another ) */
	int last_operation;
};

/**
 * @brief This function creates an SO_FILE data structure for a given valid
 * pathname and valid mode
 *
 * @param pathname path to the file
 * @param mode the opening mode
 * @return SO_FILE*
 */
SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *aux;

	/* reserve memory for SO_FILE structure */
	aux = (SO_FILE *)calloc(1, sizeof(SO_FILE));
	/* if there was an error return NULL */
	if (aux == NULL)
		return NULL;

	/* open the file with the corresponding permissions */
	if (strcmp(mode, "r+") == 0)
		aux->fd = CreateFile(pathname, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	else if (strcmp(mode, "w+") == 0)
		aux->fd = CreateFile(pathname, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	else if (strcmp(mode, "a+") == 0)
		aux->fd = CreateFile(pathname, GENERIC_READ | FILE_APPEND_DATA,	FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	else if (strcmp(mode, "r") == 0)
		aux->fd = CreateFile(pathname, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	else if (strcmp(mode, "w") == 0)
		aux->fd = CreateFile(pathname, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	else if (strcmp(mode, "a") == 0)
		aux->fd = CreateFile(pathname, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	else {
		/* if the mode isn't one of the above, return NULL */
		free(aux);
		return NULL;
	}

	/* if the file couldn't be opened free everything and return NULL */
	if (aux->fd == INVALID_HANDLE_VALUE) {
		free(aux);
		return NULL;
	}

	/* reserve memory for the buffer */
	aux->buffer = (unsigned char *)malloc(4096 * sizeof(unsigned char));
	if (aux->buffer == NULL) {
		free(aux);
		return NULL;
	}

	/* return the SO_FILE */
	return aux;
}
/**
 * @brief This function returns the file descriptor of a given SO_FILE
 * structure
 *
 * @param stream the SO_FILE structure
 * @return the file descriptor of a give SO_FILE structure
 */
HANDLE so_fileno(SO_FILE *stream) 
{ 
	return stream->fd; 
}

/**
 * @brief This function frees the SO_FILE data structure and closes the
 * file descriptor
 *
 * @param stream the SO_FILE data structure that needs to be closed
 * @return 0 - in case of success
 *         -1 - if it's an error
 */
int so_fclose(SO_FILE *stream)
{
	int return_value = 0;
	int aux;

	/* if the last operation was a write drop the bytes in the buffer */
	/* into the file */
	aux = so_fflush(stream);
	/* if there was an error during the flush, return -1 */
	if (aux != 0)
		return_value = SO_EOF;
	/* close the file descriptor */
	/* if there was an error during closing, return -1 */
	if (!CloseHandle(stream->fd))
		return_value = SO_EOF;
	/* free the buffer */
	free(stream->buffer);
	/* free the structure */
	free(stream);
	return return_value;
}

/**
 * @brief This function checks if the last operation was a write one
 * and then writes the buffered data to the file
 *
 * @param stream the SO_FILE that contains the fd in which we need to
 *        write data
 * @return -1 - if there was an error during writing
 *         0 - if the operation succeded
 */
int so_fflush(SO_FILE *stream)
{
	/* check if there is data in the buffer and if the last operation */
	/* on the structure was a write one */
	if (stream->start > 0 && stream->last_operation == 2) {
		BOOL ret;
		int bytes_to_dump = stream->start;
		int bytes_wrote;
		/* while there are still bytes that need to be written, try to
		 */
		/* write them in the file */
		while (bytes_to_dump) {
			/*bytes_wrote = write(stream->fd,
					    (stream->buffer) + stream->start -
						bytes_to_dump,
					    bytes_to_dump);*/
			ret = WriteFile(stream->fd, (stream->buffer) + stream->start -
						bytes_to_dump, bytes_to_dump, &bytes_wrote, NULL);
			if (ret == FALSE) {
				stream->start = 0;
				stream->error = 2;
				return SO_EOF;
			}
			bytes_to_dump -= bytes_wrote;
		}
	}
	/* reset the buffer cursor to zero and mark that the last operation */
	/* was neither a write one nor a read one */
	/* that means that there is no need for another flush or fseek */
	stream->start = 0;
	stream->last_operation = 0;
	return 0;
}

/**
 * @brief This function moves the offset of the open channel structure
 * to the corresponding offset, but not before invalidating the buffer
 * ( in case the last operation was a read one ) or flushing the buffer
 * ( in case the last operation was a write one )
 *
 * @param stream the SO_FILE data structure
 * @param offset the offset the cursor needs to be moved
 * @param whence the starting point of the moving (start of file/current
 * position/end of file)
 * @return SO_EOF - in case of error
 *         0 - in case of success
 */
int so_fseek(SO_FILE *stream, long offset, int whence)
{
	int val;
	/* if the last operation was a write */
	if (stream->last_operation == 2) {
		/* write the buffered bytes to the file */
		int aux = so_fflush(stream);
		/* return -1 in case of error */
		if (aux)
			return SO_EOF;
	}
	/* if the last operation was a read one */
	else if (stream->last_operation == 1) {
		/* move the offset of the open channel structure to the last */
		/* character that was put in the buffer but not read */
		long aux = SetFilePointer(stream->fd, stream->start - stream->len_read, NULL, FILE_CURRENT); /*lseek(stream->fd, stream->start - stream->len_read,
				SEEK_CUR);*/
				
		/* return -1 in case of failure */
		if (aux == INVALID_SET_FILE_POINTER)
			return SO_EOF;
		/* reset the starting positions of the buffer */
		stream->start = 0;
		stream->len_read = 0;
	}
	/* mark that the last operation was neither a write one nor a read one
	 */
	stream->last_operation = 0;
	/* move to the position given as argument */
	val = SetFilePointer(stream->fd, offset, NULL, whence);
	/* return -1 in case of error */
	if (val == INVALID_SET_FILE_POINTER)
		return SO_EOF;
	return 0;
}

/**
 * @brief This function returns the offset of the open channel structure offset
 *
 * @param stream the SO_FILE data structure
 * @return result of the lseek
 */
long so_ftell(SO_FILE *stream)
{
	/* before the actual lseek we need to do a so_fseek to free the */
	/* structure buffer */
	so_fseek(stream, 0, SEEK_CUR);
	/* return the offset, zero positions to the right of the current offset
	 */
	return SetFilePointer(stream->fd, 0, NULL, SEEK_CUR);/*lseek(stream->fd, 0, SEEK_CUR);*/
}

/**
 * @brief This function reads bytes from a file ( more than needed )
 * and stores them to a buffer. Then it copies exactly the number of bytes
 * requested to ptr.
 *
 * @param ptr the zone in which we need to put the read bytes
 * @param size the size of one element
 * @param nmemb the number of elements
 * @param stream the SO_FILE data structure
 * @return 0 - in case of error
 *         number of elements read - in case of success
 */
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int aux = 0;
	char character_value;

	/* copy bytes to ptr till aux >= nmemb * size or till we encountered an
	 */
	/* error */
	while ((aux < nmemb * size) && stream->error == 0) {
		/* get the result of the so_fgetc function */
		character_value = (unsigned char)so_fgetc(stream);

		/* if the function finished, but we encountered an error while
		 */
		/* reading or we reached the end of file, stop copying bytes */
		if (stream->error == 1 || stream->error == 2)
			break;
		memcpy(((unsigned char *)ptr) + aux, (stream->buffer) + (stream->start - 1), 1);
		aux++;
	}
	/* if there was an error, return 0 */
	if (stream->error == 2)
		return 0;
	/* return the number of elements put into ptr */
	return aux / size;
}

/**
 * @brief This function writes bytes from ptr to the buffer of the file.
 * If the buffer is full, write the bytes from the buffer to the file.
 *
 * @param ptr the zone in which we need to read the bytes from
 * @param size the size of one element
 * @param nmemb the number of elements
 * @param stream the SO_FILE data structure
 * @return 0 - in case of an error
 *         number of elements read - if the function finished correctly
 */
size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int aux = 0;
	char character_value;

	/* copy bytes to SO_FILE buffer till aux >= nmemb * size or till we */
	/* encountered an error */
	while ((aux < nmemb * size) && stream->error == 0) {
		character_value = (unsigned char)so_fputc(
		    (int)(((unsigned char *)ptr)[aux]), stream);
		aux++;
	}
	/* if there was an error, return 0 */
	if (stream->error == 2)
		return 0;
	/* return the number of elements put into ptr */
	return aux / size;
}

/**
 * @brief This function reads bytes from the file and stores them in an internal
 * buffer
 *
 * @param stream the SO_FILE data structure
 * @return -1 - if there was an error or we reached EOF
 *         first character not "read" but written in the buffer - if everything
 * was ok
 */
int so_fgetc(SO_FILE *stream)
{
	/* if the buffer is empty or we read the last character put in buffer */
	/* we need to read bytes again */
	if (stream->start == stream->len_read) {
		int bytes_read;
		BOOL ret;
		stream->start = 0;
		stream->len_read = 0;
		/* read bytes_read bytes */
		ret = ReadFile(stream->fd, stream->buffer, 4096, &bytes_read, NULL);
		//bytes_read = read(stream->fd, stream->buffer, 4096);

		/* mark the last operation as a read one */
		stream->last_operation = 1;
		/* if we received 0 bytes, we reached EOF */
		if (ret == TRUE && bytes_read == 0) {
			stream->error = 1;
			return SO_EOF;
		}
		/* if bytes_read is -1, that means there was an error */
		else if (ret == FALSE) {
			stream->error = 2;
			return SO_EOF;
		}
		/* else actualize the number of bytes read */
		/* move to the next byte and return the byte before */
		else {
			stream->len_read = bytes_read;
			stream->error = 0;
			stream->start++;
			return (int)stream->buffer[stream->start - 1];
		}
	}
	/* there are still bytes in buffer */
	/* move to the next byte and return the byte before */
	else {
		stream->start++;
		stream->last_operation = 1;
		return (int)stream->buffer[stream->start - 1];
	}
}

/**
 * @brief This function receives a character and puts it in the
 * buffer of the SO_FILE structure to be later written in the file
 *
 * @param c character to be written
 * @param stream the SO_FILE data structure
 * @return -1 - if there was an error
 *         the character received - if everything was ok
 */
int so_fputc(int c, SO_FILE *stream)
{
	unsigned char aux = (unsigned char)c;

	/* mark that the last operation was a write one */
	stream->last_operation = 2;
	/* if we reached the end of the buffer, flush it in the file */
	if (stream->start == 4096) {
		int res = so_fflush(stream);

		if (!res) {
			memcpy((stream->buffer) + (stream->start), &aux, 1);
			stream->start++;
		}
		/* if there was an error, return an error code and mark that */
		/* there was an error */
		else {
			stream->error = 2;
			return SO_EOF;
		}
	}
	/* else copy the value to the buffer */
	else {
		memcpy((stream->buffer) + (stream->start), &aux, 1);
		stream->start++;
	}
	return aux;
}

/**
 * @brief This function checks if we reached the end of file
 *
 * @param stream the SO_FILE data structure
 * @return -1 - if we reached EOF
 *         0 - if we didn't reach EOF
 */
int so_feof(SO_FILE *stream)
{
	if (stream->error == 1)
		return SO_EOF;
	return 0;
}

/**
 * @brief This function checks if the there was a reading/writing error
 *
 * @param stream the SO_FILE data structure
 * @return -1 - if there was an error
 *         0 - if everything is still ok
 */
int so_ferror(SO_FILE *stream)
{
	if (stream->error == 2)
		return SO_EOF;
	return 0;
}

/**
 * @brief 
 * 
 * @param psi 
 * @param hFile 
 * @param opt 
 * @return VOID 
 */
static VOID RedirectHandle(STARTUPINFO *psi, HANDLE hFile, INT opt)
{
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	ZeroMemory(psi, sizeof(*psi));
	psi->cb = sizeof(*psi);

	psi->hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	psi->hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	psi->hStdError = GetStdHandle(STD_ERROR_HANDLE);

	psi->dwFlags |= STARTF_USESTDHANDLES;

	switch (opt) {
	case STD_INPUT_HANDLE:
		psi->hStdInput = hFile;
		break;
	case STD_OUTPUT_HANDLE:
		psi->hStdOutput = hFile;
		break;
	case STD_ERROR_HANDLE:
		psi->hStdError = hFile;
		break;
	}
}

/**
 * @brief This function creates a new proccess that executes the command given
 * as argument. It returns a SO_FILE data structure based on the type argument.
 *
 * @param command the command that needs to be executed
 * @param type the type of the resulting file
 * @return NULL - if there was an error
 *         an SO_FILE data structure - if everything was ok
 */
SO_FILE *so_popen(const char *command, const char *type)
{
	
}

/**
 * @brief This function frees the SO_FILE data structure and waits
 * for the child proccess to finish
 *
 * @param stream the SO_FILE data structure
 * @return -1 - if there was an error
 *         exit code of the child proccess - if everything was ok
 */
int so_pclose(SO_FILE *stream)
{
	
}