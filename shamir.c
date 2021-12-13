#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/random.h>

extern uint8_t GF256_2exp[256];
extern uint8_t GF256_log2[256];

unsigned char reading = 0;
unsigned char writing = 0;

int num_shares = 0;
int num_required = 0;
char *input_file_names[255];
unsigned int num_input_files = 0;
uint8_t *input_data;
size_t input_data_size;
FILE *output_files[255];

char *output_file_name = NULL;
FILE *input_files[255];
uint8_t input_x_vals[255];
FILE *output_file;

uint8_t GF256_mult(uint8_t a, uint8_t b){
	if(a && b){
		return GF256_2exp[(GF256_log2[a] + GF256_log2[b])%255];
	} else {
		return 0;
	}
}

uint8_t GF256_inverse(uint8_t a){
	return GF256_2exp[255 - GF256_log2[a]];
}

void parse_args(int argc, char **argv){
	int i;

	for(i = 1; i < argc; i++){
		if(!strcmp(argv[i], "-s")){
			i++;
			if(i < argc){
				num_shares = atoi(argv[i]);
			} else {
				fprintf(stderr, "Error: Expected number of shares.\n");
				exit(1);
			}
		} else if(!strcmp(argv[i], "-n")){
			i++;
			if(i < argc){
				num_required = atoi(argv[i]);
			} else {
				fprintf(stderr, "Error: Expected number of required shares.\n");
				exit(1);
			}
		} else if(!strcmp(argv[i], "-r")){
			reading = 1;
		} else if(!strcmp(argv[i], "-w")){
			writing = 1;
		} else if(!strcmp(argv[i], "-o")){
			i++;
			if(i < argc){
				output_file_name = argv[i];
			} else {
				fprintf(stderr, "Error: Expected output file name.\n");
				exit(1);
			}
		} else {
			if(num_input_files >= 255){
				fprintf(stderr, "Error: At most 255 shares can be read.\n");
				exit(1);
			}
			input_file_names[num_input_files] = argv[i];
			num_input_files++;
		}
	}
}

uint8_t *read_file(char *file_name, size_t *file_size){
	FILE *fp;
	uint8_t *output;

	fp = fopen(file_name, "rb");
	if(!fp){
		fprintf(stderr, "Error: Cannot open file '%s' for reading.\n", file_name);
		exit(1);
	}
	fseek(fp, 0, SEEK_END);
	*file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	output = malloc(sizeof(uint8_t)*(*file_size));
	if(!output){
		fprintf(stderr, "Error: malloc failed.\n");
		exit(1);
	}
	if(fread(output, sizeof(uint8_t), *file_size, fp) < *file_size){
		free(output);
		fprintf(stderr, "Error: Cannot read file '%s'.\n", file_name);
		exit(1);
	}
	fclose(fp);

	return output;
}

void init_output_files(){
	char output_file_name[256];
	int i;
	int j;

	for(i = 0; i < num_shares; i++){
		snprintf(output_file_name, 256, "%d_%s", i, input_file_names[0]);
		output_files[i] = fopen(output_file_name, "wb");
		if(!output_files[i]){
			for(j = 0; j < i; j++){
				fclose(output_files[j]);
			}
			free(input_data);
			fprintf(stderr, "Error: Cannot open file '%s' for output.\n", output_file_name);
			exit(1);
		}
		fputc(num_required, output_files[i]);
		fputc(i + 1, output_files[i]);
	}
}

void init_input_files(){
	int i;
	int j;
	int share_num_required;
	size_t share_input_data_size;

	for(i = 0; i < num_input_files; i++){
		input_files[i] = fopen(input_file_names[i], "rb");
		if(!input_files[i]){
			for(j = 0; j < i; j++){
				fclose(input_files[j]);
			}
			fprintf(stderr, "Error: Cannot open file '%s' for reading.\n", input_file_names[i]);
			exit(1);
		}
		share_num_required = fgetc(input_files[i]);
		if(!i){
			fseek(input_files[0], 0, SEEK_END);
			input_data_size = ftell(input_files[0]);
			fseek(input_files[0], 1, SEEK_SET);
			num_required = share_num_required;
			input_x_vals[0] = fgetc(input_files[0]);
		} else {
			if(share_num_required != num_required){
				for(j = 0; j <= i; j++){
					fclose(input_files[j]);
				}
				fprintf(stderr, "Error: Incompatible shares.\n");
				exit(1);
			}
			fseek(input_files[i], 0, SEEK_END);
			share_input_data_size = ftell(input_files[i]);
			fseek(input_files[i], 1, SEEK_SET);
			if(share_input_data_size != input_data_size){
				for(j = 0; j <= i; j++){
					fclose(input_files[j]);
				}
				fprintf(stderr, "Error: Incompatible shares.\n");
				exit(1);
			}
			input_x_vals[i] = fgetc(input_files[i]);
		}
	}

	if(input_data_size < 3){
		for(j = 0; j < num_input_files; j++){
			fclose(input_files[j]);
		}
		fprintf(stderr, "Error: Malformed shares.\n");
		exit(1);
	}

	if(num_input_files < num_required){
		for(j = 0; j < num_input_files; j++){
			fclose(input_files[j]);
		}
		fprintf(stderr, "Error: Not enough shares to recover data.\n");
		exit(1);
	}

	input_data_size -= 2;
}

void write_shares(){
	uint8_t coefficients[256];
	uint8_t power;
	uint8_t value;
	long i;
	int j;
	int k;

	for(i = 0; i < input_data_size; i++){
		coefficients[0] = input_data[i];
		if(getrandom(coefficients + 1, sizeof(uint8_t)*num_required, 0) < sizeof(uint8_t)*num_required){
			for(j = 0; j < num_shares; j++){
				fclose(output_files[j]);
			}
			free(input_data);
			fprintf(stderr, "Error: Call to getrandom failed.\n");
			exit(1);
		}
		for(k = 0; k < num_shares; k++){
			value = 0;
			power = 1;
			for(j = 0; j < num_required; j++){
				value ^= GF256_mult(coefficients[j], power);
				power = GF256_mult(power, k + 1);
			}
			fputc(value, output_files[k]);
		}
	}
}

void write_secret(){
	int i;
	int j;
	int k;
	uint8_t sum;
	uint8_t prod;

	for(i = 0; i < input_data_size; i++){
		sum = 0;
		for(j = 0; j < num_input_files; j++){
			prod = 1;
			for(k = 0; k < num_input_files; k++){
				if(j == k){
					continue;
				}
				prod = GF256_mult(prod, GF256_mult(input_x_vals[k], GF256_inverse(input_x_vals[j]^input_x_vals[k])));
			}
			prod = GF256_mult(fgetc(input_files[j]), prod);
			sum ^= prod;
		}
		fputc(sum, output_file);
	}
}

void close_output(){
	int i;

	for(i = 0; i < num_shares; i++){
		fclose(output_files[i]);
	}
}

void close_input(){
	int i;

	for(i = 0; i < num_input_files; i++){
		fclose(input_files[i]);
	}
}

int main(int argc, char **argv){
	parse_args(argc, argv);

	if(!reading && !writing){
		fprintf(stderr, "Error: Use '-r' to read shares, '-w' to write shares.\n");
		return 1;
	}

	if(reading && writing){
		fprintf(stderr, "Error: Use at most one of '-r' and '-w'.\n");
		return 1;
	}

	if(writing){
		if(!num_shares){
			fprintf(stderr, "Error: Use '-s' to specify the number of shares.\n");
			return 1;
		}
		if(!num_required){
			fprintf(stderr, "Error: Use '-n' to specify the number fo required shares.\n");
			return 1;
		}
		if(num_input_files != 1){
			fprintf(stderr, "Error: Exactly one input file must be given.\n");
			return 1;
		}

		if(num_shares > 255){
			fprintf(stderr, "Error: There can be at most 255 shares.\n");
			return 1;
		}

		input_data = read_file(input_file_names[0], &input_data_size);
		init_output_files();
		write_shares();
		close_output();
		free(input_data);

		return 0;
	} else if(reading){
		if(!output_file_name){
			fprintf(stderr, "Error: Expected output file name.\n");
			return 1;
		}
		init_input_files();
		output_file = fopen(output_file_name, "wb");
		if(!output_file){
			close_input();
			fprintf(stderr, "Error: Cannot open file '%s' for writing.\n", output_file_name);
			return 1;
		}
		write_secret();
		close_input();
		fclose(output_file);

		return 0;
	}
}

