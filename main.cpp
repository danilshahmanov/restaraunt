#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <iostream>
#include <soci/soci.h>
#include <string>
#include <soci/sqlite3/soci-sqlite3.h>
#include <optional>
#include <numeric>
namespace http = web::http;
void send_response(web::http::http_request& request, web::json::value& data, web::http::status_code status){

  web::http::http_response response(status);
  response.headers().add(U("Access-Control-Allow-Origin"), U("*")); // Allow requests from any origin
  response.headers().add(U("Access-Control-Allow-Methods"), U("GET, POST")); // Allow GET and POST methods
  response.headers().add(U("Access-Control-Allow-Headers"), U("Content-Type")); // Allow the Content-Type header
  response.set_body(data);

  request.reply(response);
}
class Dish {
public:
  std::optional<int> id;
  std::optional<std::string> name;
  std::optional<double> price;
  std::optional<int> weight;
  Dish() : id(std::nullopt), name(std::nullopt), price(std::nullopt), weight(std::nullopt) {};
  Dish(std::string name, double price, int weight): id(std::nullopt), name(name), price(price), weight(weight) {};
  Dish(int id, std::optional<std::string> name, std::optional<double> price, std::optional<int> weight) : 
  id(id), name(name), price(price), weight(weight) {};
};

class DishRepository {
private:
  soci::session sql;
public:
  DishRepository(){
    try {
        sql.open(*soci::factory_sqlite3(), "C:/c++/restaraunt/database/dishes.db");
        CreateTable();
    } catch (const std::exception& e) {
        std::cerr << "Failed to open the database: " << e.what() << std::endl;
    }
  };
  std::list<Dish> GetAll(){
    std::list<Dish> dishes;
    soci::rowset<soci::row> rs = (sql.prepare << "SELECT id, name, price, weight FROM dishes");
    for (auto it = rs.begin(); it != rs.end(); ++it) {
        soci::row const& row = *it;
        Dish dish(
          row.get<int>("id"),
          row.get<std::string>("name"),
          row.get<double>("price"),
          row.get<int>("weight")
        );
        dishes.push_back(dish);
    }
    return dishes;
  }
  std::list<double> GetAllPrices(){
    std::list<double> prices;
    soci::rowset<soci::row> rs = (sql.prepare << "SELECT price FROM dishes");
    for (auto it = rs.begin(); it != rs.end(); ++it) {
        soci::row const& row = *it;
        prices.push_back(row.get<double>("price"));
    }
    return prices;
  }
  void Update(const Dish& dish){
    std::stringstream queryStream;
    queryStream << "UPDATE dishes SET ";
    if(dish.name.has_value()) {
      queryStream << "name = :name,";
    }
    if(dish.price.has_value()){
      queryStream << "price = :price,";
    }
    if(dish.weight.has_value()){
      queryStream << "weight = :weight,";
    }
    std::string query = queryStream.str();
    query.pop_back();
    query += " WHERE id = :id";
    soci::statement st = (sql.prepare << query);
    if(dish.name.has_value()) {
      st.exchange(soci::use(dish.name.value()));
    }
    if(dish.price.has_value()){
      st.exchange(soci::use(dish.price.value()));
    }
    if(dish.weight.has_value()){
      st.exchange(soci::use(dish.weight.value()));
    }
    st.exchange(soci::use(dish.id.value()));
    st.define_and_bind();
    //if true: performs the query and commits it immediately, making the changes permanent in the database
    st.execute(true);   
    if (st.get_affected_rows() == 0) {
        throw std::invalid_argument("dish not found");
    }   
  }
  bool Exists(const Dish& dish){
    int existingDishes;
    soci::statement st = (sql.prepare << "SELECT COUNT(*) FROM dishes WHERE name = :name AND price = :price",
    soci::into(existingDishes), soci::use(dish.name.value()), soci::use(dish.price.value()));
    st.execute(true);
    return existingDishes!=0;
  }
  Dish GetById(const int id){
    Dish dish;
    soci::indicator ind;
    //soci::indicator ind is paired with dish.id because fetching a dish based on its id. 
    //If the id is not found or is NULL, it's a strong indication that the user doesn't exist or the data is incomplete
    soci::statement st = (sql.prepare << "SELECT id, name, price, weight FROM dishes WHERE id = :id",
    soci::into(*(dish.id), ind),soci::into(*(dish.name)), soci::into(*(dish.price)), soci::into(*(dish.weight)), soci::use(id));  
    st.execute(true);
    if (ind == soci::i_ok) {
        return dish;
    } else {
        throw std::invalid_argument("dish not found");
    }
  }
  void DeleteById(const int id){
    soci::statement st = (sql.prepare << "DELETE FROM dishes WHERE id = :id", soci::use(id));
    st.execute(true); 
    if (st.get_affected_rows() == 0) {
        throw std::invalid_argument("dish not found");
    }
  }
  void Add(const Dish& dish) {
    if(this->Exists(dish)){
      throw std::invalid_argument("dish already exists");
    }
    soci::statement st = (sql.prepare << "INSERT INTO dishes (name, price, weight) VALUES (:name, :price, :weight)",
    soci::use(dish.name.value()), soci::use(dish.price.value()), soci::use(dish.weight.value()));
    st.execute(true);
  }

  void CreateTable(){
    try{
     // SQL query to create the Dish table if it doesn't exist
        std::string createTableQuery = R"(
            CREATE TABLE IF NOT EXISTS dishes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                price REAL NOT NULL,
                weight INTEGER NOT NULL
            )
        )";

        // Execute the query to create the table
        sql << createTableQuery;

    } catch (const soci::soci_error& e) {
        // Handle exceptions
        std::cerr << "Error: " << e.what() << std::endl;
  }
  }
};
class DishService{
  private:
    DishRepository dishRepository;
  public:
  DishService()=default;
  void CreateDish(std::string name, double price, int weight){
    Dish dish(name, price, weight);
    dishRepository.Add(dish);
  }
  void DeleteDishById(int id){
    dishRepository.DeleteById(id);
  }
  void UpdateDish(int id, std::optional<std::string> name, std::optional<double> price, std::optional<int> weight){
    Dish dish(id, name, price, weight);
    dishRepository.Update(dish);
  }
  std::list<Dish> GetAllDishes(){
    return dishRepository.GetAll();
  }
  double GetAveragePrice(){
    std::list<double> prices = dishRepository.GetAllPrices();
     if (prices.empty()) {
        throw std::invalid_argument("there are no dishes");
    }   
    double sum = std::accumulate(prices.begin(), prices.end(), 0.0);
    return sum / static_cast<double>(prices.size());
  }
};

class DishController{
  private:
  DishService dishService;
  public:
  DishController()=default;
  void HandleAddDishRequest(web::http::http_request& request) {
    auto requestData = request.extract_json().get();
    const auto& hasMandatoryFields = [&requestData]() -> bool {
        return requestData.has_field(U("name")) && requestData.has_field(U("price")) && requestData.has_field(U("weight"));
    };
    web::json::value responseData;
    if (!hasMandatoryFields()){ 
      responseData[U("error")] = web::json::value::string(U("missing mandatory fields"));
      send_response(request, responseData, web::http::status_codes::BadRequest);
    }
    try {
      dishService.CreateDish(
        utility::conversions::to_utf8string(requestData.at(U("name")).as_string()), 
        requestData.at(U("price")).as_double(), 
        requestData.at(U("weight")).as_integer()
      );
      responseData[U("success")] = web::json::value::string(U("dish was added successfully"));
      send_response(request, responseData, web::http::status_codes::OK);
    } catch (const std::invalid_argument& e) {
      responseData[U("error")] = web::json::value::string(utility::conversions::to_string_t(e.what()));
      send_response(request, responseData, web::http::status_codes::BadRequest);
    } catch (const soci::soci_error& e) {
      responseData[U("error")] = web::json::value::string(utility::conversions::to_string_t(e.what()));
      send_response(request, responseData, web::http::status_codes::BadRequest);
    }
  }
  void HandleGetAveragePriceRequest(web::http::http_request request){
    web::json::value responseData = web::json::value::object();
    try {
        int averagePrice = dishService.GetAveragePrice();
        responseData[U("data")] = web::json::value::number(averagePrice);
        send_response(request, responseData, web::http::status_codes::OK);
    } catch (const std::invalid_argument& e) {
      responseData[U("error")] = web::json::value::string(utility::conversions::to_string_t(e.what()));
      send_response(request, responseData, web::http::status_codes::BadRequest);
    } catch (const soci::soci_error& e) {
      responseData[U("error")] = web::json::value::string(utility::conversions::to_string_t(e.what()));
      send_response(request, responseData, web::http::status_codes::BadRequest);
    }
  }
  void HandleDeleteDishRequest(web::http::http_request request) {
    web::json::value responseData;
    try {
          auto path = request.relative_uri().path();
    auto id_str = path.substr(path.find_last_of('/') + 1);
    int id = std::stoi(id_str);
      dishService.DeleteDishById(id);
      responseData[U("success")] = web::json::value::string(U("dish was deleted successfully"));
      send_response(request, responseData, web::http::status_codes::OK);
    } catch (const std::invalid_argument& e) {
      responseData[U("error")] = web::json::value::string(U("invalid id"));
      send_response(request, responseData, web::http::status_codes::BadRequest);
    } catch (const soci::soci_error& e) {
      responseData[U("error")] = web::json::value::string(utility::conversions::to_string_t(e.what()));
      send_response(request, responseData, web::http::status_codes::BadRequest);
    }
  }
  void HandleGetAllDishesRequest(web::http::http_request request) {
    web::json::value responseData = web::json::value::object();
    web::json::value arrayOfDishes = web::json::value::array();
    try {
        std::list<Dish> dishes = dishService.GetAllDishes();
        int index = 0;
        for (const auto& dish : dishes) {
            web::json::value dishObject = web::json::value::object();
            dishObject[U("id")] = web::json::value::number(dish.id.value());
            dishObject[U("name")] = web::json::value::string(utility::conversions::to_string_t(dish.name.value()));
            dishObject[U("price")] = web::json::value::number(dish.price.value());
            dishObject[U("weight")] = web::json::value::number(dish.weight.value());
            arrayOfDishes[index++] = dishObject;
        }
        responseData[U("success")] = web::json::value::string(U("dishes loaded successfully"));
        responseData[U("data")] = arrayOfDishes;
        send_response(request, responseData, web::http::status_codes::OK);
    } catch (const std::exception& e) {
      responseData[U("error")] = web::json::value::string(utility::conversions::to_string_t(e.what()));
      send_response(request, responseData, web::http::status_codes::BadRequest);
    } catch (const soci::soci_error& e) {
      responseData[U("error")] = web::json::value::string(utility::conversions::to_string_t(e.what()));
      send_response(request, responseData, web::http::status_codes::BadRequest);
    }
}
void HandleUpdateDishRequest(web::http::http_request request){
    auto requestData = request.extract_json().get();
    web::json::value responseData;
   int id;
     try {
          auto path = request.relative_uri().path();
    auto id_str = path.substr(path.find_last_of('/') + 1);
    id = std::stoi(id_str);}
     catch (const std::invalid_argument& e) {
      responseData[U("error")] = web::json::value::string(U("invalid id"));
      send_response(request, responseData, web::http::status_codes::BadRequest);}
    try {
      std::optional<std::string> name;
      if(requestData.has_field(U("name"))){ name = utility::conversions::to_utf8string(requestData.at(U("name")).as_string());} 
      else{name=std::nullopt;};
      std::optional<double> price;
      if(requestData.has_field(U("price"))){ price = requestData.at(U("price")).as_double();} 
      else{price=std::nullopt;};
      std::optional<int> weight;
      if(requestData.has_field(U("weight"))){ weight = requestData.at(U("weight")).as_integer();} 
      else{weight=std::nullopt;};
      dishService.UpdateDish(
        id, name, price, weight
      );
      responseData[U("success")] = web::json::value::string(U("dish was updated successfully"));
      send_response(request, responseData, web::http::status_codes::OK);
    } catch (const std::invalid_argument& e) {
      responseData[U("error")] = web::json::value::string(utility::conversions::to_string_t(e.what()));
      send_response(request, responseData, web::http::status_codes::BadRequest);
    } catch (const soci::soci_error& e) {
      responseData[U("error")] = web::json::value::string(utility::conversions::to_string_t(e.what()));
      send_response(request, responseData, web::http::status_codes::BadRequest);
    }
}
};

class RequestHandler {
  private:
    web::http::experimental::listener::http_listener listener; 
    DishController dishController;
   
void general_handler(web::http::http_request request) {
    auto path = request.relative_uri().path();
    auto paths = web::uri::split_path(path);
    if (path == U("/dishes") && request.method() == web::http::methods::GET) {
        dishController.HandleGetAllDishesRequest(request);
    } else if (paths.size() == 2 && paths[0] == U("dish") && request.method() == web::http::methods::DEL) {
        dishController.HandleDeleteDishRequest(request);
    } else if (path == U("/dishes") && request.method() == web::http::methods::POST){
        dishController.HandleAddDishRequest(request);
    }else if (paths.size() == 2 && paths[0] == U("dish") && request.method() == web::http::methods::PUT) {
        dishController.HandleUpdateDishRequest(request);
    }else if(path == U("/dishes/average_price") && request.method() == web::http::methods::GET){
        dishController.HandleGetAveragePriceRequest(request);
    }
    else{
      request.reply(web::http::status_codes::NotFound);
    }
}
public:
  RequestHandler() : listener(web::http::experimental::listener::http_listener(U("http://localhost:8080"))) {
    //register methods in listener
    
    listener.support( [this](http::http_request request) {
        this->general_handler(request);
      });
    

    // CORS support
    listener.support(web::http::methods::OPTIONS, [](http::http_request request) {
      http::http_response response(http::status_codes::OK);
      response.headers().add(U("Access-Control-Allow-Origin"), U("*"));
      response.headers().add(U("Access-Control-Allow-Methods"), U("GET, POST, OPTIONS, PUT, DELETE"));
      response.headers().add(U("Access-Control-Allow-Headers"), U("Content-Type, Authorization"));
      request.reply(response);
    });
  }

  void open() {
    listener.open().wait();
    std::wcout << U("Listening on port 8080. Press any key to terminate.") << std::endl; 
    std::string exit_symbol;
    std::cin >> exit_symbol;
  }

  void close() {
    listener.close().wait();
    std::wcout << U("Listener closed. Exiting.") << std::endl;
  }
  ~RequestHandler(){
    close();
  }
};


int main() {
  RequestHandler server;
  server.open();
 
    return 0;
}



