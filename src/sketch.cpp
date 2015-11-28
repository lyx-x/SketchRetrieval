/*
 * Sketch-based 3D model retrieval
 *
 * This program is based on Eitz et al. paper, it has two component: a offline one and an online one. In the main
 * structure, we will code the online part which take a sketch as input and output a 3D model. The offline part is
 * coded in other project under the folder "utils/".
 *
 * Whenever you want to use this program, run the offline part first, which builds a database for online query.
 *
 * Parameters:
 *      d: TF-IDF database of all existing views information
 *      w: dictionary file generated by K-Means
 *      l: label file for all existing views
 *      m: folder to all PLY models
 *      c: camera mode
 *      f: file mode with input image file path
 *
 * Usage 1 (file based query):
 *      sketch -d [database_file] -w [dictionary_file] -l [label_file] -m [model_folder] -f [input_file]
 *
 * Usage 2 (real-time query with camera):
 *      sketch -d [database_file] -w [dictionary_file] -l [label_file] -m [model_folder] -c
 */

#include <iostream>

#include <vtkPolyData.h>
#include <vtkPLYReader.h>
#include <vtkSmartPointer.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>

#include "tf_idf.h"
#include "clusters.h"

enum Mode {Camera, File, Testing};
Mode mode = Testing;

string database_file, label_file, input_file, dictionary_file;

string model_base = "/home/lyx/workspace/data/TinySketch/models_ply/";

vector<Mat> kernels;

int k = 8; // number of Gabor filters
int kernel_size = 15;
double sigma = 4;
double theta = 0;
double lambda = 10.0;
double beta = 0.5;
int window_size = 8; // local feature area (not size)
int point_per_row = 28;
int center_count = 0;
int dim = 0;
int feature_count = point_per_row * point_per_row; // number of features per image

void show_help();

void read_dict(Clusters &dict);

int retrieve(Mat& image, Clusters& dictionary); // return the index of model

void show_model(string file); // show 3D model with VTK

int to_index(int label);

string to_name(int index); // build 3D model name given the index

bool parse_command_line(int argc, char **argv); // Process all arguments

int main(int argc, char** argv) {

    if (!parse_command_line(argc, argv))
        return EXIT_FAILURE;

    int model_index = -1;

    ifstream input(dictionary_file);
    input.read((char*) &center_count, sizeof(int));
    input.read((char*) &dim, sizeof(int));

    float *centers = new float[center_count * dim];
    input.read((char*) centers, sizeof(float) * center_count * dim);
    input.close();

    Clusters dict(centers, center_count, dim);

    if (mode == File) {
        Mat image_gray = imread(input_file, CV_LOAD_IMAGE_GRAYSCALE);
        Mat image;
        image_gray.convertTo(image, CV_32FC1);
        int label = retrieve(image, dict);

        cout << label << endl;
        model_index = to_index(label);
        cout << model_index << endl;
    }
    else if (mode == Camera) {

    }
    else { // Testing mode
        //show_model(to_name(87));
    }

    delete[] centers;

    return EXIT_SUCCESS;
}
void kernel(){
    double step = CV_PI / k;

    for(int i = 0; i < k; i++) {
        Mat kernel = getGaborKernel(Size(kernel_size, kernel_size), sigma,
                                    theta + step * (double) i, lambda, beta, CV_PI * 0.5, CV_32F);
        kernels.push_back(kernel);
    }
}

void gabor_filter(Mat& img , float *data){

    if (kernels.empty())
        kernel();

    vector<Mat> filters(k);

    for(int i = 0; i < k; i++)
        filter2D(img, filters[i], -1, kernels[i], Point(-1, -1), 0, BORDER_DEFAULT);

    int index = 0;
    int row_gap = (img.rows - window_size) / point_per_row;
    int col_gap = (img.cols - window_size) / point_per_row;

    for(int i = 0; i < img.rows - window_size; i += row_gap)
        for(int j = 0; j < img.cols - window_size; j += col_gap)
            for(int dir = 0; dir < k; dir++)
                for(int u = 0; u < window_size; u++)
                    for(int v = 0; v < window_size; v++)
                        data[index++] = filters[dir].at<float>(u + i, v + j);

}
// return the index of model
int retrieve(Mat& image, Clusters& dictionary) {

    // use Gabor filter
    int dim = window_size * window_size * k;
    float *gabor_data = new float[feature_count * dim];
    gabor_filter(image, gabor_data);

    // translate into words
    int* word = new int[feature_count];
    dictionary.find_center(gabor_data, word, feature_count);

    // compute TF-IDF
    //ofstream output("word.bin");
    //output.write((char*)word, sizeof(float) * feature_count *dim);
    //output.close();

    TF_IDF tf_idf(database_file);

    //cout << tf_idf.get_word_count() << endl;

    // get nearest neighbor
    int *tf_value = new int[center_count];
    for(int i = 0; i < feature_count; i++)
        tf_value[word[i]]++;

    int result = tf_idf.find_nearest(tf_value);

    delete[] tf_value;
    delete[] word;
    delete[] gabor_data;

    return result;
}

// show 3D model with VTK
void show_model(string file) {

    vtkSmartPointer<vtkPLYReader> reader = vtkSmartPointer<vtkPLYReader>::New();
    reader->SetFileName(file.c_str());

    // Visualize
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(reader->GetOutputPort());

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);

    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    renderWindowInteractor->SetRenderWindow(renderWindow);

    renderer->AddActor(actor);
    renderer->SetBackground(0, 0, 0);

    renderWindow->Render();
    renderWindowInteractor->Start();

}

// build 3D model name given the index
string to_name(int index) {
    return model_base + "m" + to_string(index) + ".ply";
}

int to_index(int label){
    ifstream input(label_file);
    int model_count, view_count;
    input >> model_count >> view_count;
    int k = 0;
    int tmp;
    input >> tmp;
    while(k + view_count <= label){
        input >> tmp;
        k = k + view_count;
    }
    return tmp;
}

// Process all arguments
bool parse_command_line(int argc, char **argv) {

    int i = 1;
    while(i < argc) {
        if (argv[i][0] != '-')
            break;
        switch(argv[i][1]) {
            case 'h': // help
                show_help();
                return false;
            case 'd': // TF-IDF file
                database_file = argv[++i];
                break;
            case 'w': // dictionary file
                dictionary_file = argv[++i];
                break;
            case 'l': // label file
                label_file = argv[++i];
                break;
            case 'm': // folder containing all PLY models
                model_base = argv[++i];
                break;
            case 'f': // input sktech image
                mode = File;
                input_file = argv[++i];
                break;
            case 'c': // camera mode
                mode = Camera;
                break;
            case 'p':
                feature_count = atoi(argv[++i]);
                break;
        }
        i++;
    }
    if (database_file == "" || dictionary_file == "")
        return false;
    return true;
}

void show_help() {
}

